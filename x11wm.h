#ifndef X11WM_H
#define X11WM_H

#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <xcb/xcb.h>

namespace xcb
{
class xcb_atom
{
public:
    static xcb_atom_t get_atom(xcb_connection_t *connection, const char *name)
    {
        xcb_atom_t atom = XCB_ATOM_NONE;

        // error
        xcb_generic_error_t *err;

        xcb_intern_atom_cookie_t atom_cookie = xcb_intern_atom(connection, 1, static_cast<uint16_t>(strlen(name)), name);
        xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(connection, atom_cookie, &err);
        if (err)
        {
            fprintf(stderr, "xcb_intern_atom [atom: %s] error: %d\n", name, err->error_code);
            free(err);
            return atom;
        }

        // ok
        if (atom_reply)
        {
            atom = atom_reply->atom;
            free(atom_reply);
        }
        return atom;
    }
};

class xcb_window
{
    xcb_atom_t wm_class_atom;
    xcb_atom_t net_wm_name_atom;
    xcb_atom_t net_wm_desktop_atom;
    xcb_atom_t net_current_desktop_atom;
    xcb_window_t window;
    xcb_window_t root_window;
    xcb_connection_t *connection;

    std::string wm_title;
    std::string wm_class;
    std::string wm_instance;
    uint32_t wm_desktop;

public:
    xcb_window(xcb_connection_t *c, xcb_window_t win, xcb_window_t root)
        : window(win), root_window(root), connection(c)
    {
        wm_class_atom = xcb_atom::get_atom(connection, "WM_CLASS");
        net_wm_name_atom = xcb_atom::get_atom(connection, "_NET_WM_NAME");
        net_wm_desktop_atom = xcb_atom::get_atom(connection, "_NET_WM_DESKTOP");
        net_current_desktop_atom = xcb_atom::get_atom(connection, "_NET_CURRENT_DESKTOP");
    }

    inline xcb_window_t wmWindow() const { return window; }
    inline std::string wmTitle() const { return wm_title; }
    inline std::string wmClass() const { return wm_class; }
    inline std::string wmInstance() const { return wm_instance; }
    inline uint32_t wmDesktop() const { return wm_desktop; }

    void update()
    {
        if (window == XCB_WINDOW_NONE)
            return;

        std::vector<std::string> props = get_window_property_class();
        if (props.size() > 1)
            wm_class = props[1];
        if (props.size() > 0)
            wm_instance = props[0];

        wm_title = get_window_property_title();
        wm_desktop = get_window_property_desktop();
    }

    void focus()
    {
        if (window == XCB_WINDOW_NONE)
            return;

        xcb_void_cookie_t void_cookie;

        // switch desktop
        const uint32_t data2[] = { wmDesktop(), XCB_CURRENT_TIME };
        size_t data_len = sizeof(data2);

        xcb_client_message_event_t ev;
        memset(&ev, 0, sizeof(xcb_client_message_event_t));

        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.window = root_window;
        ev.format = 32;
        ev.type = net_current_desktop_atom;

        memcpy(ev.data.data32, data2, data_len);

        void_cookie = xcb_send_event(connection, 0, root_window, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                                     (char *)&ev);

        // map the window
        void_cookie = xcb_map_window(connection, window);

        // set input focus
        void_cookie = xcb_set_input_focus(connection, XCB_INPUT_FOCUS_PARENT, window, XCB_CURRENT_TIME);

        // restack the window
        const uint32_t data[] = { XCB_STACK_MODE_ABOVE };
        xcb_configure_window(connection, window, XCB_CONFIG_WINDOW_STACK_MODE, data);

        // flush
        xcb_flush(connection);
    }

    bool hasTitle(std::string title) const
    {
        return wm_title == title;
    }

    bool hasClass(std::string clas, std::string inst) const
    {
        return wm_class == clas && wm_instance == inst;
    }

private:
    std::vector<std::string> get_window_property_class()
    {
        std::vector<std::string> class_instance;

        // error
        xcb_generic_error_t *err;

        xcb_get_property_cookie_t prop_cookie = xcb_get_property(connection, 0, window, wm_class_atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 1024);
        xcb_get_property_reply_t *reply_prop = xcb_get_property_reply(connection, prop_cookie, &err);
        if (err)
        {
            fprintf(stderr, "xcb_get_property [atom: %d] [window: 0x%08x] error: %d\n", wm_class_atom, window, err->error_code);
            free(err);
            return class_instance;
        }

        // ok
        if (reply_prop)
        {
            int value_len = xcb_get_property_value_length(reply_prop);
            if (value_len)
            {
                const char *prop = static_cast<const char *>(xcb_get_property_value(reply_prop));

                int bytes = 0;
                while (bytes < value_len)
                {
                    class_instance.push_back(std::string(prop, size_t(strnlen(prop, size_t(value_len)))));
                    int size = int(class_instance.back().length()) + 1;
                    bytes += size;
                    prop += size;
                }
            }
            free(reply_prop);
        }
        return class_instance;
    }

    std::string get_window_property_title()
    {
        std::string title;

        // error
        xcb_generic_error_t *err;

        xcb_get_property_cookie_t prop_cookie = xcb_get_property(connection, 0, window, net_wm_name_atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 1024);
        xcb_get_property_reply_t *reply_prop = xcb_get_property_reply(connection, prop_cookie, &err);
        if (err)
        {
            fprintf(stderr, "xcb_get_property [atom: %d] [window: 0x%08x] error: %d\n", net_wm_name_atom, window, err->error_code);
            free(err);
            return title;
        }

        // ok
        if (reply_prop)
        {
            int value_len = xcb_get_property_value_length(reply_prop);
            if (value_len)
            {
                const char *prop = static_cast<const char *>(xcb_get_property_value(reply_prop));
                title = std::string(prop, size_t(strnlen(prop, size_t(value_len))));
            }
            free(reply_prop);
        }
        return title;
    }

    uint32_t get_window_property_desktop()
    {
        uint32_t desktop = 0;

        // error
        xcb_generic_error_t *err;

        xcb_get_property_cookie_t prop_cookie = xcb_get_property(connection, 0, window, net_wm_desktop_atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 128);
        xcb_get_property_reply_t *reply_prop = xcb_get_property_reply(connection, prop_cookie, &err);
        if (err)
        {
            fprintf(stderr, "xcb_get_property [atom: %d] [window: 0x%08x] error: %d\n", net_wm_desktop_atom, window, err->error_code);
            free(err);
            return desktop;
        }

        // ok
        if (reply_prop)
        {
            int value_len = xcb_get_property_value_length(reply_prop);
            if (value_len)
            {
                const char *prop = static_cast<const char *>(xcb_get_property_value(reply_prop));
                memcpy(&desktop, prop, sizeof(uint32_t));
            }
            free(reply_prop);
        }
        return desktop;
    }
};

class xcb_desktop
{
    xcb_atom_t net_client_list_atom;
    xcb_window_t root_window;
    xcb_connection_t *connection;

public:
    xcb_desktop()
        : connection(nullptr)
    {
        // connect to X server
        connection = xcb_connect(nullptr, nullptr);

        // _NET_CLIENT_LIST atom
        net_client_list_atom = xcb_atom::get_atom(connection, "_NET_CLIENT_LIST");

        // root window id
        root_window = get_root_window();
    }
    ~xcb_desktop()
    {
        // disconnect from X server
        xcb_disconnect(connection);
    }

public:
    std::vector<xcb_window> get_client_list()
    {
        // windowsy;
        std::vector<xcb_window> windows;

        //err
        xcb_generic_error_t *err;

        // get
        xcb_get_property_cookie_t prop_cookie_list = xcb_get_property(connection, 0, root_window, net_client_list_atom, XCB_ATOM_WINDOW, 0, 1024);
        xcb_get_property_reply_t *reply_prop_list = xcb_get_property_reply(connection, prop_cookie_list, &err);

        // fail
        if (err)
        {
            fprintf(stderr, "xcb_get_property [atom: %d] [window: 0x%08x] error: %d\n", net_client_list_atom, root_window, err->error_code);
            free(err);
            return windows;
        }

        // ok
        if (reply_prop_list)
        {
            int bytes = reply_prop_list->format / 8;
            int value_len = xcb_get_property_value_length(reply_prop_list);
            if (value_len && bytes)
            {
                xcb_window_t *win = static_cast<xcb_window_t *>(xcb_get_property_value(reply_prop_list));
                for (int i = 0; i < value_len / bytes; i++)
                {
                    windows.push_back(xcb_window(connection, win[i], root_window));
                }
            }
            free(reply_prop_list);
        }
        return windows;
    }

private:
    xcb_window_t get_root_window()
    {
        const xcb_setup_t *setup = xcb_get_setup(connection);
        if (setup == nullptr)
            return XCB_WINDOW_NONE;
        xcb_screen_t *first_screen = xcb_setup_roots_iterator(setup).data;
        if (first_screen == nullptr)
            return XCB_WINDOW_NONE;
        return first_screen->root;
    }
};

class xcb_master
{
    xcb_desktop desktop;

public:
    void focus_by_title(std::string wmtitle)
    {
        std::vector<xcb_window> windows = desktop.get_client_list();

        auto it = std::find_if(windows.begin(), windows.end(), [&](xcb_window &w) {
            w.update();
            return w.hasTitle(wmtitle);
        });

        if (it == windows.end())
            return;

        it->focus();
    }
    void focus_by_class(std::string wmclass, std::string wminstance)
    {
        std::vector<xcb_window> windows = desktop.get_client_list();

        auto it = std::find_if(windows.begin(), windows.end(), [&](xcb_window &w) {
            w.update();
            return w.hasClass(wmclass, wminstance);
        });

        if (it == windows.end())
            return;

        it->focus();
    }
};

} // namespace xcb

#endif // X11WM_H
