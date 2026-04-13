//
// Created by unnamedfurry on 4/13/26.
//
#include <gtk/gtk.h>

static void activate(GtkApplication *app, gpointer user_data){
    GtkBuilder *builder;
    GtkWidget *window;
    GError *error = NULL;
    builder = gtk_builder_new_from_file("client.ui"); // loading ui file
    // errors catch
    /*builder = gtk_builder_new();
    if (!gtk_builder_add_from_file(builder, "client.ui", &error)) {
        g_printerr("Error UI loading: %s\n", error->message);
        g_error_free(error);
        g_object_unref(builder);
        return;
    }*/
    window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window")); // getting main window by id
    gtk_window_set_application(GTK_WINDOW(window), app); // linking window with app (required)
    gtk_window_present(GTK_WINDOW(window)); // showing window
    g_object_unref(builder); // freeing window (required)
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.unnamedfurry.unchat", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}