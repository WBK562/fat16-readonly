#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_reader.h"

#define ROZMIAR_SEKTORA 512


struct clusters_chain_t *get_chain_fat16(void* buffer, size_t size, uint16_t first_cluster) {
    if (buffer == NULL || size < 1 || first_cluster < 1 || first_cluster >= size){return NULL;}
    struct clusters_chain_t *chain = malloc(sizeof(struct clusters_chain_t));
    if (chain == NULL){return NULL;}

    const uint16_t *temp = (const uint16_t*)buffer;
    chain->clusters = NULL;
    chain->size = 0;

    uint16_t klaster = first_cluster;
    while (klaster < 0xfff8) {
        chain->clusters = realloc(chain->clusters,sizeof(uint16_t) * (chain->size + 1));
        *(chain->clusters+chain->size) = klaster;
        chain->size++;
        klaster = *(uint16_t*)(temp + klaster);
    }
    return chain;
}

void sprowadzenie_do_normalnej_nazwy(const char *source, char *nazwa_pliku) {
    int i,j=0;
    for (i = 0;i < 8;i++) {
        if(*(source+i) != ' ') {
            *(nazwa_pliku+j) = *(source+i);
            j++;
        }
    }
    *(nazwa_pliku+j) = '.';
    int zapisany = j;
    int ilosc_znakow = 0;
    j++;
    for (i = 8;i < 11;i++) {
        if(*(source+i) != ' ') {
            *(nazwa_pliku+j) = *(source+i);
            j++;
        }
        if (isalnum(*(source+i))) {
            ilosc_znakow++;
        }
    }
    if (ilosc_znakow) zapisany = j;
    *(nazwa_pliku+zapisany) = '\0';
}

struct disk_t* disk_open_from_file(const char* volume_file_name) {
    if(volume_file_name == NULL){errno = EFAULT; return NULL;}
    FILE *f1 = fopen(volume_file_name,"rb");
    if(f1 == NULL) {
        errno = ENOENT;
        return NULL;
    }
    struct disk_t *disk = malloc(sizeof(struct disk_t));
    if(!disk){errno = ENOMEM; return NULL;}
    disk->dysk_f = f1;
    return disk;
}

int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read) {
    if(pdisk == NULL || buffer == NULL || sectors_to_read < 1){errno = EFAULT;return -1;}
    fseek(pdisk->dysk_f,first_sector*ROZMIAR_SEKTORA,SEEK_SET);
    size_t ilosc_wczytanych = fread(buffer,ROZMIAR_SEKTORA,sectors_to_read,pdisk->dysk_f);
    if (ilosc_wczytanych != (size_t)sectors_to_read){errno = ERANGE;return -1;}
    return (int)ilosc_wczytanych;
}

int disk_close(struct disk_t* pdisk) {
    if(pdisk == NULL){errno = EFAULT; return -1;}
    fclose(pdisk->dysk_f);
    free(pdisk);
    return 0;
}

struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector) {
    if (pdisk == NULL){errno = EFAULT;return NULL;}
    struct volume_t *wolumin = malloc(sizeof(struct volume_t));
    if (!wolumin) { errno = ENOMEM; return NULL; }
    void *bufor = malloc(sizeof(uint32_t) * 512);
    if (!bufor) {free(wolumin); errno = ENOMEM; return NULL; }
    if (disk_read(pdisk, (int32_t)first_sector, bufor, 1) != 1) {
        errno = EINVAL;
        free(bufor);
        free(wolumin);
        return NULL;
    }
    wolumin->disk = pdisk;
    wolumin->bytes_per_sector = *(int16_t*)((uint8_t*)bufor + 0x0B);
    wolumin->sectors_per_cluster = *(uint8_t*)((uint8_t*)bufor + 0x0D);
    wolumin->reserved_sectors = *(uint16_t*)((uint8_t*)bufor + 0x0E);
    wolumin->fat_count = *(uint8_t*)((uint8_t*)bufor + 0x10);
    wolumin->sectors_per_fat = *(uint16_t*)((uint8_t*)bufor + 0x16);
    wolumin->root_dir_capacity = *(uint16_t*)((uint8_t*)bufor + 0x11);
    wolumin->logical_sectors16 = *(uint16_t*)((uint8_t*)bufor + 0x13);
    wolumin->logical_sectors32 = *(uint32_t*)((uint8_t*)bufor + 0x20);
    wolumin->magiczna_liczba = *(uint16_t*)((uint8_t*)bufor + 0x1FE);
    wolumin->volume_start = first_sector;

    //walidacja tego wszystkiego
    if (wolumin->fat_count != 1 && wolumin->fat_count != 2) {
        errno = EINVAL;
        free(bufor);
        free(wolumin);
        return NULL;
    }
    if (wolumin->magiczna_liczba != 0xaa55 || wolumin->reserved_sectors < 1) {
        errno = EINVAL;
        free(bufor);
        free(wolumin);
        return NULL;
    }
    if (wolumin->logical_sectors16 == 0 && wolumin->logical_sectors32 <= 65535 || (wolumin->logical_sectors16 == 0) == (wolumin->logical_sectors32 == 0)){
        errno = EINVAL;
        free(bufor);
        free(wolumin);
        return NULL;
    }
    if (wolumin->root_dir_capacity * 32 % wolumin->bytes_per_sector != 0) {
        errno = EINVAL;
        free(bufor);
        free(wolumin);
        return NULL;
    }
    wolumin->fat1_start = wolumin->volume_start + wolumin->reserved_sectors;
    wolumin->fat2_start = wolumin->fat1_start + wolumin->sectors_per_fat;
    wolumin->root_start = wolumin->fat1_start + wolumin->fat_count * wolumin->sectors_per_fat;
    wolumin->sector_per_root = wolumin->root_dir_capacity * 32 / wolumin->bytes_per_sector;
    wolumin->data_start = wolumin->root_start + wolumin->sector_per_root;
    free(bufor);
    return wolumin;
}

int fat_close(struct volume_t* pvolume) {
    if (!pvolume) {errno = EFAULT; return -1;}
    free(pvolume);
    return 0;
}

struct file_t* file_open(struct volume_t* pvolume, const char* file_name) {
    if (pvolume == NULL){errno = EFAULT; return NULL;}
    if (file_name == NULL){errno = ENOENT; return NULL;}
    uint8_t *bufor = calloc(512,sizeof(uint8_t));
    if (!bufor){errno = ENOMEM; return NULL;}
    uint8_t *bufor_plikow = NULL;
    char nazwa_pliku[13];
    uint8_t ilosc_sektorow = pvolume->bytes_per_sector / 32;
    for (size_t j = 0; j < pvolume->sector_per_root; j++) {
        disk_read(pvolume->disk, (int)(pvolume->root_start+j), bufor, 1);
        for (uint8_t i = 0;i < ilosc_sektorow;i++) {
            bufor_plikow = bufor + (i * 32);
            if (*bufor_plikow == 0x00){break;}
            if (*bufor_plikow == 0x5E){continue;}
            sprowadzenie_do_normalnej_nazwy((const char*)bufor_plikow,nazwa_pliku);
            if (strcmp(nazwa_pliku,file_name) == 0) {
                uint8_t kat = *(bufor_plikow + 0x0B);
                if (kat & 0x10) {
                    free(bufor);
                    errno = EISDIR;
                    return NULL;
                }
                struct file_t *file = malloc(sizeof(struct file_t));
                if (!file) {
                    free(bufor);
                    free(bufor_plikow);
                    errno = ENOMEM;
                    return NULL;
                }
                strcpy(file->name,file_name);
                file->pos = 0;
                file->volume = pvolume;
                file->size = *(int*)(bufor_plikow + 28);
                file->first_cluster = *(uint16_t*)(bufor_plikow + 0x1A);
                size_t rozmiar_test = pvolume->sectors_per_fat * pvolume->bytes_per_sector;
                uint8_t *chain_clust = malloc(rozmiar_test);
                if (disk_read(pvolume->disk, pvolume->fat1_start, chain_clust, pvolume->sectors_per_fat) != pvolume->sectors_per_fat) {
                    return NULL;
                }
                file->chain_cluster = get_chain_fat16(chain_clust,pvolume->sectors_per_fat * pvolume->bytes_per_sector,file->first_cluster);
                free(chain_clust);
                free(bufor);
                return file;
            }
        }
    }
    errno = ENOENT;
    free(bufor);
    return NULL;
}

int file_close(struct file_t* stream) {
    if (stream == NULL){errno = EFAULT; return -1;}
    free(stream->chain_cluster->clusters);
    free(stream->chain_cluster);
    free(stream);
    return 0;
}

size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream) {
    if (ptr == NULL || stream == NULL){errno = EFAULT; return  -1;}
    if (size == 0 || nmemb == 0){return 0;}
    size_t total_size = nmemb * size;
    size_t file_size = stream->size;
    size_t final_size = (total_size > file_size - stream->pos) ? file_size - stream->pos : total_size;
    size_t check_i = 0,check_j = 0;
    size_t sektor_size = ROZMIAR_SEKTORA;
    while ((int32_t)sektor_size <= stream->pos) {
        check_j++;
        if (check_j == stream->volume->sectors_per_cluster) {
            check_j = 0;
            check_i++;
        }
        sektor_size += ROZMIAR_SEKTORA;
    }
    size_t copied = 0;
    size_t i = check_i,j = check_j;
    uint8_t *bufor = malloc(ROZMIAR_SEKTORA * stream->volume->sectors_per_cluster);
    uint8_t *bufor_koncowy = (uint8_t *)ptr;
    while (final_size > 0 && i <= stream->chain_cluster->size * stream->volume->sectors_per_cluster) {
        if (j == stream->volume->sectors_per_cluster){ i++;j = 0; }
        uint32_t sektor = stream->volume->data_start + (stream->chain_cluster->clusters[i] - 2) * stream->volume->sectors_per_cluster + j;
        disk_read(stream->volume->disk, sektor, bufor,stream->volume->sectors_per_cluster);
        size_t pointer = stream->pos % ROZMIAR_SEKTORA;
        if(copied > 0) pointer = 0;
        size_t max_kopia = ROZMIAR_SEKTORA - pointer;
        size_t how_many_copy = (final_size < max_kopia) ? final_size : max_kopia;
        for (size_t k = 0;k < how_many_copy;k++,copied++) {
            *(bufor_koncowy+copied) = *(bufor+pointer+k);
        }
        final_size -= how_many_copy;
        j++;
    }
    stream->pos += (int32_t)copied;
    free(bufor);
    return copied / size;
}

int32_t file_seek(struct file_t* stream, int32_t offset, int whence) {
    if(!stream) {errno = EFAULT; return -1;}
    int new_pos = 0;
    if(whence == SEEK_SET) {
        if (offset < 0 || offset > stream->size) {
            errno = ENXIO;
            return -1;
        }
        new_pos = offset;
    }
    else if (whence == SEEK_CUR) {
        if (stream->pos + offset < 0 || stream->pos + offset > stream->size) {
            errno = ENXIO;
            return -1;
        }
        new_pos = stream->pos + offset;
    }
    else if (whence == SEEK_END) {
        if (offset > 0 || stream->size + offset < 0) {
            errno = ENXIO;
            return -1;
        }
        new_pos = stream->size + offset;
    }
    else{
        errno = EINVAL;
        return -1;
    }
    stream->pos = new_pos;
    return stream->pos;
}

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path) {
    if (pvolume == NULL || dir_path == NULL){errno = EFAULT; return NULL;}
    if (strcmp(dir_path,"\\") != 0){errno = ENOENT;return NULL;}
    struct dir_t* dir = malloc(sizeof(struct dir_t));
    if (!dir) { errno = ENOMEM; return NULL; }
    uint8_t bufor[512];
    disk_read(pvolume->disk,pvolume->root_start,bufor,1);
    uint8_t kat = *(bufor + 0x0B);
    if (!(kat & 0x10)) {
        errno = ENOTDIR;
        return NULL;
    }
    dir->wolumin = pvolume;
    dir->bajt32 = 0;
    dir->sektor = 0;
    return dir;
}

int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry) {
    if (pdir == NULL || pentry == NULL){errno = EFAULT; return -1;}
    if (pdir->sektor >= pdir->wolumin->sector_per_root) {errno = ENXIO; return -1;}
    uint8_t *bufor32;
    uint8_t bufor[512];
    while(pdir->sektor < pdir->wolumin->sector_per_root) {
        if (disk_read(pdir->wolumin->disk, pdir->wolumin->root_start + pdir->sektor, bufor, 1) != 1) {
            errno = EIO;
            return -1;
        }
        while(pdir->bajt32 < 16) {
            bufor32 = bufor + pdir->bajt32 * 32;
            if (*bufor32 == 0x00){break;}
            if (*bufor32 == 0xE5){pdir->bajt32++; continue;}
            sprowadzenie_do_normalnej_nazwy((const char*)bufor32,pentry->name);
            uint8_t odczyt_11 = *(bufor+11);
            pentry->is_readonly = odczyt_11 & 0x01 ? 1 : 0;
            pentry->is_hidden = odczyt_11 & 0x02 ? 1 : 0;
            pentry->is_system = odczyt_11 & 0x04 ? 1 : 0;
            pentry->is_directory = odczyt_11 & 0x10 ? 1 : 0;
            pentry->is_archived = odczyt_11 & 0x20 ? 1 : 0;
            pentry->size = *(int*)(bufor+28);
            pdir->bajt32++;
            return 0;
        }
        pdir->bajt32 = 0;
        pdir->sektor++;
    }
    return 1;
}

int dir_close(struct dir_t* pdir) {
    if (!pdir) { errno = EFAULT; return -1; }
    free(pdir);
    return 0;
}

int main(void){
    struct disk_t *disk = disk_open_from_file("laugh_fat16_volume.img");
    if(!disk){
        printf("Niepopranie otworzono plik!");
        return -1;
    }
    struct volume_t *volume = fat_open(disk,0);
    if(!volume){
        printf("Funckja fat_open() nie zwrocila struktury volume");
        free(disk);
        return -2;
    }
    struct file_t *file = file_open(volume,"KILL.TXT");
    if(!file){
        printf("Funkcja file_open() niepoprawnie otworzyla plik!");
        fat_close(volume);
        disk_close(disk);
        return -3;
    }
    char *filecontent = (char *)calloc(10240, 1);
    char expected_filecontent[10241] = "";

    strcat(expected_filecontent, "tions rise\nCapraia and Gorgona, and dam up\nThe mouth of Arno, that each soul in thee\nMay perish in the waters!  What if fame\nReported that thy castles were betray\'d\nBy Ugolino, yet no right hadst thou\nTo stretch his children on the rack.  For them,\nBrigata, Ugaccione, and the pair\nOf gentle ones, of whom my song hath told,\nTheir tender years, thou modern Thebes!  did make\nUncapable of guilt.  Onward we pass\'d,\nWhere others skarf\'d in rugged folds of ice\nNot on their feet were turn\'d, but each revers\'d\n     There very weeping suffers not to weep;\nFor at their eyes grief seeking passage finds\nImpediment, and rolling inward turns\nFor increase of sharp anguish:  the first tears\nHang cluster\'d, and like crystal vizors show,\nUnder the socket brimming all the cup.\n     Now though the cold had from my face dislodg\'d\nEach feeling, as \'t were callous, yet me seem\'d\nSome breath of wind I felt.  \"Whence cometh this,\"\nSaid I, \"my master?  Is not here below\nAll vapour quench\'d?\"--\"\'Thou shalt be speedily,\"\nHe answer\'d, \"where thine eye shall tell thee whence\nThe cause descrying of this airy shower.\"\n     Then cried out one in the chill crust who mourn\'d:\n\"O souls so cruel!  that the farthest post\nHath been assign\'d you, from this face remove\nThe harden\'d veil, that I may vent the grief\nImpregnate at my heart, some little space\nEre it congeal again!\"  I thus replied:\n\"Say who thou wast, if thou wouldst have mine aid;\nAnd if I extricate thee not, far down\nAs to the lowest ice may I descend!\"\n     \"The friar Alberigo,\" answered he,\n\"Am I, who from the evil garden pluck\'d\nIts fruitage, and am here repaid, the date\nMore luscious for my fig.\"--\"Hah!\"  I exclaim\'d,\n\"Art thou too dead!\"--\"How in the world aloft\nIt fareth with my body,\" answer\'d he,\n\"I am right ignorant.  Such privilege\nHath Ptolomea, that ofttimes the soul\nDrops hither, ere by Atropos divorc\'d.\nAnd that thou mayst wipe out more willingly\nThe glazed tear-drops that o\'erlay mine eyes,\nKnow that the soul, that moment she betrays,\nAs I did, yields her body to a fiend\nWho after moves and governs it at will,\nTill all its time be rounded; headlong she\nFalls to this cistern.  And perchance above\nDoth yet appear the body of a ghost,\nWho here behind me winters.  Him thou know\'st,\nIf thou but newly art arriv\'d below.\nThe years are many that have pass\'d away,\nSince to this fastness Branca Doria came.\"\n     \"Now,\" answer\'d I, \"methinks thou mockest me,\nFor Branca Doria never yet hath died,\nBut doth all natural functions of a man,\nEats, drinks, and sleeps, and putteth raiment on.\"\n     He thus:  \"Not yet unto that upper foss\nBy th\' evil talons guarded, where the pitch\nTenacious boils, had Michael Zanche reach\'d,\nWhen this one left a demon in his stead\nIn his own body, and of one his kin,\nWho with him treachery wrought.  But now put forth\nThy hand, and ope mine eyes.\"  I op\'d them not.\nIll manners were best courtesy to him.\n     Ah Genoese!  men perverse in every way,\nWith every foulness stain\'d, why from the earth\nAre ye not cancel\'d?  Such an one of yours\nI with Romagna\'s darkest spirit found,\nAs for his doings even now in soul\nIs in Cocytus plung\'d, and yet doth seem\nIn body still alive upon the earth.\n\n\n\nCANTO XXXIV\n\n\"THE banners of Hell\'s Monarch do come forth\nTowards us; therefore look,\" so spake my guide,\n\"If thou discern him.\"  As, when breathes a cloud\nHeavy and dense, or when the shades of night\nFall on our hemisphere, seems view\'d from far\nA windmill, which the blast stirs briskly round,\nSuch was the fabric then methought I saw,\n     To shield me from the wind, forthwith I drew\nBehind my guide:  no covert else was there.\n     Now came I (and with fear I bid my strain\nRecord the marvel) where the souls were all\nWhelm\'d underneath, transparent, as through glass\nPellucid the frail stem.  Some prone were laid,\nOthers stood upright, this upon the soles,\nThat on his head, a third with face to feet\nArch\'d like a bow.  When to the point we came,\nWhereat my guide was pleas\'d that I should see\nThe creature eminent in beauty once,\nHe from before me stepp\'d and made me pause.\n     \"Lo!\"  he");
    strcat(expected_filecontent, " exclaim\'d, \"lo Dis! and lo the place,\nWhere thou hast need to arm thy heart with strength.\"\n     How frozen and how faint I then became,\nAsk me not, reader!  for I write it not,\nSince words would fail to tell thee of my state.\nI was not dead nor living.  Think thyself\nIf quick conception work in thee at all,\nHow I did feel.  That emperor, who sways\nThe realm of sorrow, at mid breast from th\' ice\nStood forth; and I in stature am more like\nA giant, than the giants are in his arms.\nMark now how great that whole must be, which suits\nWith such a part.  If he were beautiful\nAs he is hideous now, and yet did dare\nTo scowl upon his Maker, well from him\nMay all our mis\'ry flow.  Oh what a sight!\nHow passing strange it seem\'d, when I did spy\nUpon his head three faces: one in front\nOf hue vermilion, th\' other two with this\nMidway each shoulder join\'d and at the crest;\nThe right \'twixt wan and yellow seem\'d:  the left\nTo look on, such as come from whence old Nile\nStoops to the lowlands.  Under each shot forth\nTwo mighty wings, enormous as became\nA bird so vast.  Sails never such I saw\nOutstretch\'d on the wide sea.  No plumes had they,\nBut were in texture like a bat, and these\nHe flapp\'d i\' th\' air, that from him issued still\nThree winds, wherewith Cocytus to its depth\nWas frozen.  At six eyes he wept:  the tears\nAdown three chins distill\'d with bloody foam.\nAt every mouth his teeth a sinner champ\'d\nBruis\'d as with pond\'rous engine, so that three\nWere in this guise tormented.  But far more\nThan from that gnawing, was the foremost pang\'d\nBy the fierce rending, whence ofttimes the back\nWas stript of all its skin.  \"That upper spirit,\nWho hath worse punishment,\" so spake my guide,\n\"Is Judas, he that hath his head within\nAnd plies the feet without.  Of th\' other two,\nWhose heads are under, from the murky jaw\nWho hangs, is Brutus:  lo!  how he doth writhe\nAnd speaks not!  Th\' other Cassius, that appears\nSo large of limb.  But night now re-ascends,\nAnd it is time for parting.  All is seen.\"\n     I clipp\'d him round the neck, for so he bade;\nAnd noting time and place, he, when the wings\nEnough were op\'d, caught fast the shaggy sides,\nAnd down from pile to pile descending stepp\'d\nBetween the thick fell and the jagged ice.\n     Soon as he reach\'d the point, whereat the thigh\nUpon the swelling of the haunches turns,\nMy leader there with pain and struggling hard\nTurn\'d round his head, where his feet stood before,\nAnd grappled at the fell, as one who mounts,\nThat into hell methought we turn\'d again.\n     \"Expect that by such stairs as these,\" thus spake\nThe teacher, panting like a man forespent,\n\"We must depart from evil so extreme.\"\nThen at a rocky opening issued forth,\nAnd plac\'d me on a brink to sit, next join\'d\nWith wary step my side.  I rais\'d mine eyes,\nBelieving that I Lucifer should see\nWhere he was lately left, but saw him now\nWith legs held upward.  Let the grosser sort,\nWho see not what the point was I had pass\'d,\nBethink them if sore toil oppress\'d me then.\n     \"Arise,\" my master cried, \"upon thy feet.\n\"The way is long, and much uncouth the road;\nAnd now within one hour and half of noon\nThe sun returns.\"  It was no palace-hall\nLofty and luminous wherein we stood,\nBut natural dungeon where ill footing was\nAnd scant supply of light.  \"Ere from th\' abyss\nI sep\'rate,\" thus when risen I began,\n\"My guide!  vouchsafe few words to set me free\nFrom error\'s thralldom.  Where is now the ice?\nHow standeth he in posture thus revers\'d?\nAnd how from eve to morn in space so brief\nHath the sun made his transit?\"  He in few\nThus answering spake:  \"Thou deemest thou art still\nOn th\' other side the centre, where I grasp\'d\nTh\' abhorred worm, that boreth through the world.\nThou wast on th\' other side, so long as I\nDescended; when I turn\'d, thou didst o\'erpass\nThat point, to which from ev\'ry part is dragg\'d\nAll heavy substance.  Thou art now arriv\'d\nUnder the hemisphere opposed to that,\nWhich the great continent doth overspread,\nAnd underneath whose canopy expir\'d\nThe Man, that was born sinless, and so liv\'d.\nThy feet are planted on the smallest sphere,");
    strcat(expected_filecontent, "\nWhose other aspect is Judecca.  Morn\nHere rises, when there evening sets:  and he,\nWhose shaggy pile was scal\'d, yet standeth fix\'d,\nAs at the first.  On this part he fell down\nFrom heav\'n; and th\' earth, here prominent before,\nThrough fear of him did veil her with the sea,\nAnd to our hemisphere retir\'d.  Perchance\nTo shun him was the vacant space left here\nBy what of firm land on this side appears,\nThat sprang aloof.\"  There is a place beneath,\nFrom Belzebub as distant, as extends\nThe vaulted tomb, discover\'d not by sight,\nBut by the sound of brooklet, that descends\nThis way along the hollow of a rock,\nWhich, as it winds with no precipitous course,\nThe wave hath eaten.  By that hidden way\nMy guide and I did enter, to return\nTo the fair world:  and heedless of repose\nWe climbed, he first, I following his steps,\nTill on our view the beautiful lights of heav\'n\nDawn, through a circular opening in the cave:\nThus issuing we again beheld the stars.\n\n\n\nNOTES TO HELL\n\nCANTO I\n\nVerse 1.  In the midway.]  That the era of the Poem is intended\nby these words to be fixed to the thirty fifth year of the poet\'s\nage, A.D. 1300, will appear more plainly in Canto XXI. where that\ndate is explicitly marked.\n\nv. 16.  That planet\'s beam.]  The sun.\n\nv. 29.  The hinder foot.]  It is to be remembered, that in\nascending a hill the weight of the body rests on the hinder foot.\n\nv. 30.  A panther.]  Pleasure or luxury.\n\nv. 36.  With those stars.]  The sun was in Aries, in which sign\nhe supposes it to have begun its course at the creation.\n\nv. 43.  A lion.]  Pride or ambition.\n\nv. 45.  A she wolf.]  Avarice.\n\nv. 56.  Where the sun in silence rests.]  Hence Milton appears to\nhave taken his idea in the Samson Agonistes:\n\n        The sun to me is dark\n               And silent as the moon, &c\nThe same metaphor will recur, Canto V. v. 29.\n        Into a place I came\n       Where light was silent all.\n\nv. 65.  When the power of Julius.] This is explained by the\ncommentators to mean \"Although it was rather late with respect to\nmy birth before Juliu");

    size_t size = file_read(filecontent,1,10240,file);
    if(size != 10240){
        printf("Funkcja file_read() niepoprawnie wczytała dane z pliku!");
        file_close(file);
        fat_close(volume);
        disk_close(disk);
        return -4;
    }
    if(memcmp(filecontent,expected_filecontent,10240) == 0){
        printf("Funkcja poprawnie odczytała zawartość pliku :)");
    }
    else{
        printf("Funkcja niepoprawnie odczytała zawartość pliku :(");
    }
    printf("\n");
    //Otwieranie katalogu glownego
    struct dir_t *dir = dir_open(volume,"\\");
    if(!dir){
        printf("Funkcja niepoprawnie otworzyła katalog główny!");
    }
    char* expected_names[12] = { "FEW.TX", "YET.TXT", "KILL.TXT", "BURN.TXT", "EVER.TXT", "WHEREIHI.BIN", "GUN", "USUAL", "CAUGHT", "FEAR", "RIDE", "MAGNETTH" };
    for (int i = 0; i < 12; ++i) {
        struct dir_entry_t entry;
        int res = dir_read(dir, &entry);
        if(res != 0){
            printf("Funkcja niepoprawnie odczytała zawartość katalogu głównego");
        }
        for (int j = 0; j < 12; ++j) {
            if(strcmp(expected_names[j],entry.name) == 0){
                printf("Funkcja znalazła folder/plik o nazwie %s\n",entry.name);
                break;
            }
        }
    }
    return 0;
}



