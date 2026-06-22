#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <unistd.h>
#include <ncurses.h>

int recursive = 0;

char path[1024];
DIR *dir = NULL;

typedef struct File{
    char name[256];
    char owner[64];
    char grp_owner[64];
    mode_t rights;
    struct File *next;
}File;

File *files = NULL;
int selected = 0;
int file_count = 0;

int editing = 0;
int perm_cursor = 0;
char status_msg[64] = "";
int status_ok = 1;

mode_t perm_flags[9] = {
    S_IROTH, S_IWOTH, S_IXOTH, //vsi
    S_IRGRP, S_IWGRP, S_IXGRP, //grupa
    S_IRUSR, S_IWUSR, S_IXUSR //usr
};

void fill_files(){
    struct dirent *entry;
    File *priveous = NULL;

    if(recursive) {
        struct stat st;
        stat(path, &st);
        struct passwd *usr = getpwuid(st.st_uid);
        struct group  *grp = getgrgid(st.st_gid);

        File *node = malloc(sizeof(File));
        node->next = NULL;

        strncpy(node->name, path, sizeof(node->name));
        strncpy(node->owner, usr ? usr->pw_name : "NULL", sizeof(node->owner));
        strncpy(node->grp_owner, grp ? grp->gr_name : "NULL", sizeof(node->grp_owner));
        node->rights = st.st_mode;

        files = node;
        file_count = 1;
        return;
    }

    while((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        char fullpath[1024];
        snprintf(fullpath, 1024, "%s/%s", path, entry->d_name);

        struct stat st;
        stat(fullpath, &st);
        struct passwd *usr = getpwuid(st.st_uid);
        struct group  *grp = getgrgid(st.st_gid);

        File *node = malloc(sizeof(File));
        node->next = NULL;

        strncpy(node->name, entry->d_name, sizeof(node->name));
        strncpy(node->owner, usr ? usr->pw_name : "NULL", sizeof(node->owner));
        strncpy(node->grp_owner, grp ? grp->gr_name : "NULL", sizeof(node->grp_owner));
        node->rights = st.st_mode;

        if(files == NULL) {
            files = node;
        } else {
            priveous->next = node;
        }
        priveous = node;
        file_count++;
    }
}


void print_perms(mode_t perms, int row, int col, int is_selected) {
    if(is_selected) attron(A_REVERSE);

    //owner
      attron(COLOR_PAIR(3));
    mvprintw(row, col, "all: %c%c%c",
        (perms & S_IROTH) ? 'r' : '-',
        (perms & S_IWOTH) ? 'w' : '-',
        (perms & S_IXOTH) ? 'x' : '-');
    attroff(COLOR_PAIR(3));

    for(int i = 0; i < 3; i++) {
        if(is_selected && editing && perm_cursor == i) attron(A_BOLD | A_UNDERLINE);
        mvprintw(row, col+5+i, "%c", (perms & perm_flags[i]) ? "rwx"[i%3] : '-');
        if(is_selected && editing && perm_cursor == i) attroff(A_BOLD | A_UNDERLINE);
    }

    //grupa
    attron(COLOR_PAIR(4));
    mvprintw(row, col+13, "grp: %c%c%c",
        (perms & S_IRGRP) ? 'r' : '-',
        (perms & S_IWGRP) ? 'w' : '-',
        (perms & S_IXGRP) ? 'x' : '-');
    attroff(COLOR_PAIR(4));

    for(int i = 0; i < 3; i++) {
        if(is_selected && editing && perm_cursor == i+3) attron(A_BOLD | A_UNDERLINE);
        mvprintw(row, col+18+i, "%c", (perms & perm_flags[i+3]) ? "rwx"[i%3] : '-');
        if(is_selected && editing && perm_cursor == i+3) attroff(A_BOLD | A_UNDERLINE);
    }

    //vsi
    attron(COLOR_PAIR(5));
    mvprintw(row, col+26, "own: %c%c%c",
        (perms & S_IRUSR) ? 'r' : '-',
        (perms & S_IWUSR) ? 'w' : '-',
        (perms & S_IXUSR) ? 'x' : '-');
    attroff(COLOR_PAIR(5));

    for(int i = 0; i < 3; i++) {
        if(is_selected && editing && perm_cursor == i+6) attron(A_BOLD | A_UNDERLINE);
        mvprintw(row, col+31+i, "%c", (perms & perm_flags[i+6]) ? "rwx"[i%3] : '-');
        if(is_selected && editing && perm_cursor == i+6) attroff(A_BOLD | A_UNDERLINE);
    }

    if(is_selected) attroff(A_REVERSE);
}

void draw_ui(){
    int max_w, max_h;
    getmaxyx(stdscr, max_h, max_w); 
    
    //glava
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(0, 0, "%-40s", "Name");
    attroff(COLOR_PAIR(1) | A_BOLD);

    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(0, 40, "%-15s", "Owner");
    attroff(COLOR_PAIR(2) | A_BOLD);

    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(0, 55, "%-15s", "Group");
    attroff(COLOR_PAIR(2) | A_BOLD);

    attron(COLOR_PAIR(4) | A_BOLD);
    mvprintw(0, 70, "Permissions");
    attroff(COLOR_PAIR(4) | A_BOLD);

    attron(COLOR_PAIR(5) | A_BOLD);
    mvhline(1, 0, '-', max_w); 
    attroff(COLOR_PAIR(5) | A_BOLD);

    //trup
    File *current = files;
    int row = 2;

    while(current != NULL) {
        if(row - 2 == selected) attron(A_REVERSE);
        mvprintw(row, 0, "%-40s %-15s %-15s", current->name, current->owner, current->grp_owner);
        if(row - 2 == selected) attroff(A_REVERSE);

        print_perms(current->rights, row, 70, row - 2 == selected);

        current = current->next;
        row++;
    }

    //noga
    attron(COLOR_PAIR(5) | A_BOLD);
    mvhline(row, 0, '-', max_w); 
    attroff(COLOR_PAIR(5) | A_BOLD);

    if(strlen(status_msg) > 0) {
        attron(status_ok ? COLOR_PAIR(6) : COLOR_PAIR(4));
        mvprintw(max_h-2, 0, "%s", status_msg);
        attroff(status_ok ? COLOR_PAIR(6) : COLOR_PAIR(4));
    }

    attron(A_BOLD);
    mvprintw(max_h-1, 0, "arrows: navigate | enter: edit | esc: quit");
    attroff(A_BOLD);

}

File* get_selected() {
    File *current = files;
    int i = 0;
    while(current != NULL) {
        if(i == selected) return current;
        current = current->next;
        i++;
    }
    return NULL;
}

void chmod_recursive(const char *dirpath, mode_t rights) {
    chmod(dirpath, rights);

    DIR *d = opendir(dirpath);
    if(!d) return;

    struct dirent *entry;
    while((entry = readdir(d)) != NULL) {
        if(strcmp(entry->d_name, ".") == 0 || 
           strcmp(entry->d_name, "..") == 0) continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);

        struct stat st;
        stat(fullpath, &st);

        if(S_ISDIR(st.st_mode)) {
            chmod_recursive(fullpath, rights);
        } else {
            chmod(fullpath, rights);
        }
    }
    closedir(d);
}

int main(int argc, char *argv[]) {

   if(argc >= 2 && strcmp(argv[1], "-r") == 0) {
        recursive = 1;
        if(argc >= 3) strcpy(path, argv[2]);
        else strcpy(path, ".");
    } else if(argc >= 2) {
        strcpy(path, argv[1]);
    } else {
        strcpy(path, ".");
    }

    struct stat st;
    if(stat(path, &st) < 0) {
        perror("Cannot access path");
        return 1;
    }

    if(!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s is not a directory\n", path);
        return 1;
    }

    dir = opendir(path);
    if(!dir) {
        perror("Error opening folder");
        return 1;
    }

    fill_files();

    initscr();
    cbreak(); //input samo en char
    noecho(); //ne izpise inputa
    keypad(stdscr, TRUE); //enablamo arrow tipke
    curs_set(0);
    start_color(); //enablamo barvo

    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(3, COLOR_BLUE, COLOR_BLACK);

    init_pair(4, COLOR_RED, COLOR_BLACK);
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);
    init_pair(6, COLOR_GREEN, COLOR_BLACK);

    while(1) {
        clear();
        draw_ui();
        refresh();

        int ch = getch();

        if(!editing) {
            if(ch == 27) {  //ce je esc prekinemo
            if(recursive) {
                File *f = get_selected();
                    if(f) chmod_recursive(path, f->rights);
                }
                break;
            }
            else if(ch == KEY_UP && selected > 0) selected--;
            else if(ch == KEY_DOWN && selected < file_count - 1) selected++;
            else if(ch == '\n') { //ce je neter gremo v edit mode
                editing = 1;
                perm_cursor = 0;
                strcpy(status_msg, "");
            }
        } else {
            if(ch == KEY_LEFT && perm_cursor > 0) perm_cursor--;
            else if(ch == KEY_RIGHT && perm_cursor < 8) perm_cursor++;
            else if(ch == ' ') { 
                File *f = get_selected();
                if(f) {
                    f->rights ^= perm_flags[perm_cursor]; //xor obrne nerabimo vedet ali je on al off

                    char fullpath[1024];
                     if(recursive) {
                        snprintf(fullpath, sizeof(fullpath), "%s", f->name);
                    } else {
                        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, f->name);
                    }

                    if(chmod(fullpath, f->rights) == 0) {
                        strcpy(status_msg, "Updated!");
                        status_ok = 1;
                    } else {
                        f->rights ^= perm_flags[perm_cursor]; //ce ne gre damo nazaj na og
                        strcpy(status_msg, "Failed!");
                        status_ok = 0;
                    }
                }
            } else if(ch == '\n') {
                editing = 0;
                perm_cursor = 0;
            }
        }
    }
    endwin();

    return 0;
}