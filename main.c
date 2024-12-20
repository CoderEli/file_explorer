#include <gtk/gtk.h>
#include <dirent.h>
#include <sys/stat.h>
#include <gio/gio.h>
#include <mntent.h>

GtkWidget *list_box;
GtkWidget *current_path_label;
GtkWidget *sidebar_list;
char current_path[1024];

static void update_file_list();

static void open_file(const char *filepath) {
    GError *error = NULL;
    char *uri = g_filename_to_uri(filepath, NULL, &error);
    
    if (uri) {
        g_app_info_launch_default_for_uri(uri, NULL, &error);
        g_free(uri);
    }
}

static void add_sidebar_item(const char *icon_name, const char *label_text, const char *path) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);

    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
    GtkWidget *label = gtk_label_new(label_text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);

    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(row), box);
    gtk_container_add(GTK_CONTAINER(sidebar_list), row);
    
    g_object_set_data_full(G_OBJECT(row), "path", g_strdup(path), g_free);
}

static void add_mounted_volumes() {
    FILE *fp = setmntent("/proc/mounts", "r");
    struct mntent *mnt;

    while ((mnt = getmntent(fp))) {
        if (strstr(mnt->mnt_dir, "/dev") == 0 && 
            strstr(mnt->mnt_dir, "/sys") == 0 && 
            strstr(mnt->mnt_dir, "/proc") == 0 && 
            strstr(mnt->mnt_dir, "/run") == 0) {
            
            char *volume_name = strrchr(mnt->mnt_dir, '/');
            if (volume_name) {
                volume_name++;
            } else {
                volume_name = mnt->mnt_dir;
            }
            
            add_sidebar_item("drive-harddisk-symbolic", volume_name, mnt->mnt_dir);
        }
    }
    
    endmntent(fp);
}

static void sidebar_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    const char *path = g_object_get_data(G_OBJECT(row), "path");
    if (path) {
        strncpy(current_path, path, sizeof(current_path));
        update_file_list();
    }
}

static void update_file_list() {
    GList *children = gtk_container_get_children(GTK_CONTAINER(list_box));
    while (children) {
        gtk_widget_destroy(GTK_WIDGET(children->data));
        children = g_list_next(children);
    }

    DIR *dir = opendir(current_path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (entry->d_name[0] == '.' && entry->d_name[1] == '\0') continue;
            
            char full_path[2048];
            snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry->d_name);
            
            struct stat st;
            stat(full_path, &st);
            
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_widget_set_margin_start(box, 12);
            gtk_widget_set_margin_end(box, 12);
            gtk_widget_set_margin_top(box, 8);
            gtk_widget_set_margin_bottom(box, 8);

            const char *icon_name = S_ISDIR(st.st_mode) ? "folder" : "text-x-generic";
            GtkWidget *icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
            
            GtkWidget *label = gtk_label_new(entry->d_name);
            gtk_label_set_xalign(GTK_LABEL(label), 0.0);
            
            gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
            
            gtk_container_add(GTK_CONTAINER(row), box);
            gtk_container_add(GTK_CONTAINER(list_box), row);
        }
        closedir(dir);
    }
    
    gtk_label_set_text(GTK_LABEL(current_path_label), current_path);
    gtk_widget_show_all(list_box);
}

static void row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    GtkWidget *box_widget = gtk_bin_get_child(GTK_BIN(row));
    GList *children = gtk_container_get_children(GTK_CONTAINER(box_widget));
    GtkWidget *label = g_list_nth_data(children, 1);
    
    const char *filename = gtk_label_get_text(GTK_LABEL(label));
    char new_path[2048];
    snprintf(new_path, sizeof(new_path), "%s/%s", current_path, filename);
    
    struct stat st;
    stat(new_path, &st);
    
    if (S_ISDIR(st.st_mode)) {
        strncpy(current_path, new_path, sizeof(current_path));
        update_file_list();
    } else {
        open_file(new_path);
    }
}

static void go_up_clicked(GtkButton *button, gpointer user_data) {
    char *last_slash = strrchr(current_path, '/');
    if (last_slash && last_slash != current_path) {
        *last_slash = '\0';
        update_file_list();
    }
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "list { background-color: @theme_base_color; }"
        "list row:hover { background-color: alpha(@theme_selected_bg_color, 0.1); }"
        "list row:selected { background-color: @theme_selected_bg_color; }"
        "headerbar { min-height: 48px; }"
        , -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Modern File Explorer");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Files");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Modern File Explorer");
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    GtkWidget *up_button = gtk_button_new_from_icon_name("go-up-symbolic", GTK_ICON_SIZE_MENU);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), up_button);
    g_signal_connect(up_button, "clicked", G_CALLBACK(go_up_clicked), NULL);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), hbox);

    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(sidebar, 200, -1);
    gtk_box_pack_start(GTK_BOX(hbox), sidebar, FALSE, FALSE, 0);

    GtkStyleContext *sidebar_style = gtk_widget_get_style_context(sidebar);
    gtk_style_context_add_class(sidebar_style, "sidebar");

    sidebar_list = gtk_list_box_new();
    gtk_box_pack_start(GTK_BOX(sidebar), sidebar_list, TRUE, TRUE, 0);
    g_signal_connect(sidebar_list, "row-activated", G_CALLBACK(sidebar_row_activated), NULL);

    add_sidebar_item("user-home-symbolic", "Home", g_get_home_dir());
    add_sidebar_item("folder-documents-symbolic", "Documents", g_build_filename(g_get_home_dir(), "Documents", NULL));
    add_sidebar_item("folder-download-symbolic", "Downloads", g_build_filename(g_get_home_dir(), "Downloads", NULL));
    add_sidebar_item("folder-pictures-symbolic", "Pictures", g_build_filename(g_get_home_dir(), "Pictures", NULL));
    add_sidebar_item("folder-music-symbolic", "Music", g_build_filename(g_get_home_dir(), "Music", NULL));
    add_sidebar_item("folder-videos-symbolic", "Videos", g_build_filename(g_get_home_dir(), "Videos", NULL));
    
    add_mounted_volumes();

    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(hbox), separator, FALSE, FALSE, 0);

    GtkWidget *main_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(hbox), main_content, TRUE, TRUE, 0);

    current_path_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(current_path_label), 0.0);
    gtk_widget_set_margin_start(current_path_label, 12);
    gtk_widget_set_margin_end(current_path_label, 12);
    gtk_widget_set_margin_top(current_path_label, 8);
    gtk_widget_set_margin_bottom(current_path_label, 8);
    gtk_box_pack_start(GTK_BOX(main_content), current_path_label, FALSE, FALSE, 0);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(main_content), scrolled, TRUE, TRUE, 0);

    list_box = gtk_list_box_new();
    gtk_container_add(GTK_CONTAINER(scrolled), list_box);
    g_signal_connect(list_box, "row-activated", G_CALLBACK(row_activated), NULL);

    strncpy(current_path, g_get_home_dir(), sizeof(current_path));
    update_file_list();

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
