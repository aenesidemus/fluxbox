// Ewmh.cc for fluxbox
// Copyright (c) 2002-2003 Henrik Kinnunen (fluxgen at user.sourceforge.net)
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

// $Id: Ewmh.cc,v 1.34 2003/12/12 15:19:22 fluxgen Exp $

#include "Ewmh.hh" 

#include "Screen.hh"
#include "Window.hh"
#include "WinClient.hh"
#include "Workspace.hh"

#include <iostream>
#include <algorithm>
#include <new>
using namespace std;

Ewmh::Ewmh() {
    createAtoms();
    enableUpdate();
}

Ewmh::~Ewmh() {
    while (!m_windows.empty()) {
        XDestroyWindow(FbTk::App::instance()->display(), m_windows.back());
        m_windows.pop_back();
    }
}

void Ewmh::initForScreen(BScreen &screen) {
    Display *disp = FbTk::App::instance()->display();


    Window wincheck = XCreateSimpleWindow(disp,
                                          screen.rootWindow().window(), 
                                          0, 0, 5, 5, 0, 0, 0);

    if (wincheck != None) {
        // store the window so we can delete it later
        m_windows.push_back(wincheck);
		
        screen.rootWindow().changeProperty(m_net_supporting_wm_check, XA_WINDOW, 32,
                                           PropModeReplace, (unsigned char *) &wincheck, 1);
        XChangeProperty(disp, wincheck, m_net_supporting_wm_check, XA_WINDOW, 32,
			PropModeReplace, (unsigned char *) &wincheck, 1);

        XChangeProperty(disp, wincheck, m_net_wm_name, XA_STRING, 8,
			PropModeReplace, (unsigned char *) "Fluxbox", strlen("Fluxbox"));
    }
	
    //set supported atoms
    Atom atomsupported[] = {
        // window properties
        m_net_wm_strut,
        m_net_wm_state,
        // states that we support:
        m_net_wm_state_sticky,
        m_net_wm_state_shaded,
	m_net_wm_state_maximized_horz,
	m_net_wm_state_maximized_vert,
	m_net_wm_state_fullscreen,

        m_net_wm_desktop,

        // root properties
        m_net_client_list,
        m_net_number_of_desktops,
        m_net_current_desktop,
        m_net_active_window,
        m_net_close_window,
        m_net_moveresize_window,
        m_net_desktop_names,
        m_net_supporting_wm_check		
    };

    screen.rootWindow().changeProperty(m_net_supported, XA_ATOM, 32,
                                       PropModeReplace, 
                                       (unsigned char *) &atomsupported, 
                                       (sizeof atomsupported)/sizeof atomsupported[0]);

	
}

void Ewmh::setupClient(WinClient &winclient) {
    updateStrut(winclient);
}

void Ewmh::setupFrame(FluxboxWindow &win) {

    Atom ret_type;
    int fmt;
    unsigned long nitems, bytes_after;
    long *data = 0;
/*	
	if (XGetWindowProperty(disp, win.clientWindow(), 
        m_net_wm_state, 0, 1, False, XA_CARDINAL, 
        &ret_type, &fmt, &nitems, &bytes_after, 
        (unsigned char **) &data) ==  Success && data) {
        flags = *data;
        setState(win, flags);
        XFree(data);
	}
*/
    if (win.winClient().property(m_net_wm_desktop, 0, 1, False, XA_CARDINAL, 
                                 &ret_type, &fmt, &nitems, &bytes_after, 
                                 (unsigned char **) &data) && data) {
        unsigned int desktop = static_cast<unsigned int>(*data);
        if (desktop == 0xFFFFFFFF && !win.isStuck())
            win.stick();
        else
            win.screen().sendToWorkspace(desktop, &win, false);

        XFree(data);
    }

}

void Ewmh::updateFrameClose(FluxboxWindow &win) {
    clearState(win);
}

void Ewmh::updateClientList(BScreen &screen) {
    size_t num=0;

    BScreen::Workspaces::const_iterator workspace_it = 
        screen.getWorkspacesList().begin();
    BScreen::Workspaces::const_iterator workspace_it_end = 
        screen.getWorkspacesList().end();
    for (; workspace_it != workspace_it_end; ++workspace_it) {
        Workspace::Windows::iterator win_it = 
            (*workspace_it)->windowList().begin();
        Workspace::Windows::iterator win_it_end = 
            (*workspace_it)->windowList().end();
        for (; win_it != win_it_end; ++win_it) {
            num += (*win_it)->numClients();
        }

    }
    // and count icons
    BScreen::Icons::const_iterator icon_it = screen.getIconList().begin();
    BScreen::Icons::const_iterator icon_it_end = screen.getIconList().end();		
    for (; icon_it != icon_it_end; ++icon_it) {
        num += (*icon_it)->numClients();
    }
	
    Window *wl = new (nothrow) Window[num];
    if (wl == 0) {
        cerr<<"Fatal: Out of memory, can't allocate for Ewmh client list"<<endl;
        return;
    }

    //start the iterator from begining
    workspace_it = screen.getWorkspacesList().begin();
    int win=0;
    for (; workspace_it != workspace_it_end; ++workspace_it) {
	
        // Fill in array of window ID's
        Workspace::Windows::const_iterator it = 
            (*workspace_it)->windowList().begin();
        Workspace::Windows::const_iterator it_end = 
            (*workspace_it)->windowList().end();		
        for (; it != it_end; ++it) {
            if ((*it)->numClients() == 1)
                wl[win++] = (*it)->clientWindow();
            else {
                // add every client in fluxboxwindow to list window list
                std::list<WinClient *>::iterator client_it = 
                    (*it)->clientList().begin();
                std::list<WinClient *>::iterator client_it_end = 
                    (*it)->clientList().end();
                for (; client_it != client_it_end; ++client_it)
                    wl[win++] = (*client_it)->window();
            }
        }
    }

    // plus iconified windows
    icon_it = screen.getIconList().begin();
    for (; icon_it != icon_it_end; ++icon_it) {
        FluxboxWindow::ClientList::iterator client_it = (*icon_it)->clientList().begin();
        FluxboxWindow::ClientList::iterator client_it_end = (*icon_it)->clientList().end();
        for (; client_it != client_it_end; ++client_it)
            wl[win++] = (*client_it)->window();
    }
    
    //number of windows to show in client list
    num = win;
    screen.rootWindow().changeProperty(m_net_client_list, 
                                       XA_CARDINAL, 32,
                                       PropModeReplace, (unsigned char *)wl, num);
	
    delete [] wl;
}

void Ewmh::updateWorkspaceNames(BScreen &screen) {
    XTextProperty text;
    const BScreen::WorkspaceNames &workspacenames = screen.getWorkspaceNames();
    const size_t number_of_desks = workspacenames.size();
	
    char *names[number_of_desks];		
	
    for (size_t i = 0; i < number_of_desks; i++) {		
        names[i] = new char[workspacenames[i].size() + 1]; // +1 for \0
        memset(names[i], 0, workspacenames[i].size());
        strcpy(names[i], workspacenames[i].c_str());
    }

    if (XStringListToTextProperty(names, number_of_desks, &text)) {
        XSetTextProperty(FbTk::App::instance()->display(), screen.rootWindow().window(),
			 &text, m_net_desktop_names);
        XFree(text.value);
    }
	
    for (size_t i = 0; i < number_of_desks; i++)
        delete [] names[i];	
}

void Ewmh::updateCurrentWorkspace(BScreen &screen) {
    size_t workspace = screen.currentWorkspaceID();
    screen.rootWindow().changeProperty(m_net_current_desktop, XA_CARDINAL, 32, PropModeReplace,
                                       (unsigned char *)&workspace, 1);

}

void Ewmh::updateWorkspaceCount(BScreen &screen) {
    size_t numworkspaces = screen.getCount();
    screen.rootWindow().changeProperty(m_net_number_of_desktops, XA_CARDINAL, 32, PropModeReplace,
                                       (unsigned char *)&numworkspaces, 1);
}

void Ewmh::updateState(FluxboxWindow &win) {
    //!! TODO
}

void Ewmh::updateLayer(FluxboxWindow &win) {
    //!! TODO _NET_WM_WINDOW_TYPE
}

void Ewmh::updateHints(FluxboxWindow &win) {
}

void Ewmh::updateWorkspace(FluxboxWindow &win) {
    int workspace = win.workspaceNumber();
    if (win.isStuck())
        workspace = 0xFFFFFFFF; // appear on all desktops/workspaces

    FluxboxWindow::ClientList::iterator it = win.clientList().begin();
    FluxboxWindow::ClientList::iterator it_end = win.clientList().end();
    for (; it != it_end; ++it) {
        (*it)->changeProperty(m_net_wm_desktop, XA_CARDINAL, 32, PropModeReplace,
                              (unsigned char *)&workspace, 1);
    }

}

// return true if we did handle the atom here
bool Ewmh::checkClientMessage(const XClientMessageEvent &ce, BScreen * screen, WinClient * const winclient) {

    if (ce.message_type == m_net_wm_desktop) {
        if (screen == 0)
            return true;
        // ce.data.l[0] = workspace number
        // valid window and workspace number?
        if (winclient == 0 || winclient->fbwindow() == 0 ||
            static_cast<unsigned int>(ce.data.l[0]) >= screen->getCount())
            return true;
		
        screen->sendToWorkspace(ce.data.l[0], winclient->fbwindow(), false);		
        return true;
    } else if (ce.message_type == m_net_wm_state) {
        if (winclient == 0 || winclient->fbwindow() == 0)
            return true;

        FluxboxWindow &win = *winclient->fbwindow();
        // ce.data.l[0] = the action (remove, add or toggle)
        // ce.data.l[1] = the first property to alter
        // ce.data.l[2] = second property to alter (can be zero)
        if (ce.data.l[0] == STATE_REMOVE) {
            setState(win, ce.data.l[1], false);
            setState(win, ce.data.l[2], false);
        } else if (ce.data.l[0] == STATE_ADD) {
            setState(win, ce.data.l[1], true);
            setState(win, ce.data.l[2], true);
        } else if (ce.data.l[0] == STATE_TOGGLE) {
            toggleState(win, ce.data.l[1]);
            toggleState(win, ce.data.l[2]);
        }

        return true;
    } else if (ce.message_type == m_net_number_of_desktops) {
        if (screen == 0)
            return true;
        // ce.data.l[0] = number of workspaces
		
        // no need to alter number of desktops if they are the same
        // or if requested number of workspace is less than zero
        if (screen->getCount() == static_cast<unsigned int>(ce.data.l[0]) || 
            ce.data.l[0] < 0)
            return true;

        if (screen->getCount() > static_cast<unsigned int>(ce.data.l[0])) {
            // remove last workspace until we have
            // the same number of workspaces
            while (screen->getCount() != static_cast<unsigned int>(ce.data.l[0])) {
                screen->removeLastWorkspace();
                if (screen->getCount() == 1) // must have at least one workspace
                    break;
            }
        } else { // add workspaces to screen until workspace count match the requested size
            while (screen->getCount() != static_cast<unsigned int>(ce.data.l[0])) {
                screen->addWorkspace();					
            }
        }
			
        return true;
    } else if (ce.message_type == m_net_current_desktop) {
        if (screen == 0)
            return true;
        // ce.data.l[0] = workspace number
		
        // prevent out of range value
        if (static_cast<unsigned int>(ce.data.l[0]) >= screen->getCount())
            return true;
        screen->changeWorkspaceID(ce.data.l[0]);
        return true;
    } else if (ce.message_type == m_net_active_window) {
		
        // make sure we have a valid window
        if (winclient == 0)
            return true;
        // ce.window = window to focus
		
        winclient->focus();
        return true;
    } else if (ce.message_type == m_net_close_window) {
        if (winclient == 0)
            return true;
        // ce.window = window to close (which in this case is the win argument)
        winclient->sendClose();
        return true;
    } else if (ce.message_type == m_net_moveresize_window) {
        if (winclient == 0 && winclient->fbwindow())
            return true;
        // ce.data.l[0] = gravity and flags
        // ce.data.l[1] = x
        // ce.data.l[2] = y
        // ce.data.l[3] = width
        // ce.data.l[4] = height
        // TODO: gravity and flags
        winclient->fbwindow()->moveResize(ce.data.l[1], ce.data.l[2],
                       ce.data.l[3], ce.data.l[4]);
        return true;
    }

    // we didn't handle the ce.message_type here
    return false;
}


bool Ewmh::propertyNotify(WinClient &winclient, Atom the_property) {
    if (the_property == m_net_wm_strut) {
        updateStrut(winclient);
        return true;
    }

    return false;
}

void Ewmh::createAtoms() {

    Display *disp = FbTk::App::instance()->display();

    m_net_supported = XInternAtom(disp, "_NET_SUPPORTED", False);
    m_net_client_list = XInternAtom(disp, "_NET_CLIENT_LIST", False);
    m_net_client_list_stacking = XInternAtom(disp, "_NET_CLIENT_LIST_STACKING", False);
    m_net_number_of_desktops = XInternAtom(disp, "_NET_NUMBER_OF_DESKTOPS", False);
    m_net_desktop_geometry = XInternAtom(disp, "_NET_DESKTOP_GEOMETRY", False);
    m_net_desktop_viewport = XInternAtom(disp, "_NET_DESKTOP_VIEWPORT", False);
    m_net_current_desktop = XInternAtom(disp, "_NET_CURRENT_DESKTOP", False);
    m_net_desktop_names = XInternAtom(disp, "_NET_DESKTOP_NAMES", False);
    m_net_active_window = XInternAtom(disp, "_NET_ACTIVE_WINDOW", False);
    m_net_workarea = XInternAtom(disp, "_NET_WORKAREA", False);
    m_net_supporting_wm_check = XInternAtom(disp, "_NET_SUPPORTING_WM_CHECK", False);
    m_net_virtual_roots = XInternAtom(disp, "_NET_VIRTUAL_ROOTS", False);

    m_net_close_window = XInternAtom(disp, "_NET_CLOSE_WINDOW", False);
    m_net_moveresize_window = XInternAtom(disp, "_NET_MOVERESIZE_WINDOW", False);
	
    // TODO: implement this one
    m_net_wm_moveresize = XInternAtom(disp, "_NET_WM_MOVERESIZE", False);

    m_net_properties = XInternAtom(disp, "_NET_PROPERTIES", False);
    m_net_wm_name = XInternAtom(disp, "_NET_WM_NAME", False);
    m_net_wm_desktop = XInternAtom(disp, "_NET_WM_DESKTOP", False);
    m_net_wm_window_type = XInternAtom(disp, "_NET_WM_WINDOW_TYPE", False);
	
    // state atom and the supported state atoms
    m_net_wm_state = XInternAtom(disp, "_NET_WM_STATE", False);
    m_net_wm_state_sticky = XInternAtom(disp, "_NET_WM_STATE_STICKY", False);
    m_net_wm_state_shaded = XInternAtom(disp, "_NET_WM_STATE_SHADED", False);
    m_net_wm_state_maximized_horz = XInternAtom(disp, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    m_net_wm_state_maximized_vert = XInternAtom(disp, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    m_net_wm_state_fullscreen = XInternAtom(disp, "_NET_WM_STATE_FULLSCREEN", False);
    
    m_net_wm_strut = XInternAtom(disp, "_NET_WM_STRUT", False);
    m_net_wm_icon_geometry = XInternAtom(disp, "_NET_WM_ICON_GEOMETRY", False);
    m_net_wm_icon = XInternAtom(disp, "_NET_WM_ICON", False);
    m_net_wm_pid = XInternAtom(disp, "_NET_WM_PID", False);
    m_net_wm_handled_icons = XInternAtom(disp, "_NET_WM_HANDLED_ICONS", False);

    m_net_wm_ping = XInternAtom(disp, "_NET_WM_PING", False);
}


void Ewmh::setFullscreen(FluxboxWindow &win, bool value) {
    // fullscreen implies maximised, above dock layer, 
    // and no decorations (or decorations offscreen)
    WindowState *saved_state = getState(win);
    if (value) {
        // fullscreen on
        if (!saved_state) { // not already fullscreen
            saved_state = new WindowState(win.x(), win.y(), win.width(),
                                          win.height(), win.layerNum(), win.decorationMask());
            saveState(win, saved_state);

            // actually make it fullscreen

            // clear decorations
            win.setDecorationMask(0);

            // be xinerama aware
            BScreen &screen = win.screen();
            int head = screen.getHead(win.fbWindow());
            win.moveResize(screen.getHeadX(head), screen.getHeadY(head),
                           screen.getHeadWidth(head), screen.getHeadHeight(head));
            win.moveToLayer(Fluxbox::instance()->getAboveDockLayer());
        }
    } else { // turn off fullscreen
        if (saved_state) { // no saved state, can't restore it
            win.setDecorationMask(saved_state->decor);
            win.moveResize(saved_state->x, saved_state->y,
                           saved_state->width, saved_state->height);
            win.moveToLayer(saved_state->layer);
            clearState(win);
            saved_state = 0;
        }
    }
}

// set window state
void Ewmh::setState(FluxboxWindow &win, Atom state, bool value) {

    if (state == m_net_wm_state_sticky) { // STICKY
        if (value && !win.isStuck() ||
            (!value && win.isStuck()))
            win.stick();
    } else if (state == m_net_wm_state_shaded) { // SHADED
        if ((value && !win.isShaded()) ||
            (!value && win.isShaded()))
            win.shade();
    }  else if (state == m_net_wm_state_maximized_horz ) { // maximized Horizontal
        if ((value && !win.isMaximized()) ||
            (!value && win.isMaximized()))
	    	win.maximizeHorizontal();
    } else if (state == m_net_wm_state_maximized_vert) { // maximized Vertical
        if ((value && !win.isMaximized()) ||
            (!value && win.isMaximized()))
		win.maximizeVertical();
    } else if (state == m_net_wm_state_fullscreen) { // fullscreen
        setFullscreen(win, value);
    }
}

// toggle window state
void Ewmh::toggleState(FluxboxWindow &win, Atom state) {
    if (state == m_net_wm_state_sticky) {
        win.stick();
    } else if (state == m_net_wm_state_shaded){
        win.shade();
    } else if (state == m_net_wm_state_maximized_horz ) { // maximized Horizontal
        win.maximizeHorizontal();
    } else if (state == m_net_wm_state_maximized_vert) { // maximized Vertical
        win.maximizeVertical();
    } else if (state == m_net_wm_state_fullscreen) { // fullscreen
        setFullscreen(win, getState(win) == 0); // toggle current state
    }
}


void Ewmh::updateStrut(WinClient &winclient) {
    Atom ret_type = 0;
    int fmt = 0;
    unsigned long nitems = 0, bytes_after = 0;
    long *data = 0;
    if (winclient.property(m_net_wm_strut, 0, 4, False, XA_CARDINAL,
                                 &ret_type, &fmt, &nitems, &bytes_after,
                                 (unsigned char **) &data) && data) {
#ifdef DEBUG
        cerr<<__FILE__<<"("<<__FUNCTION__<<"): Strut: "<<data[0]<<", "<<data[1]<<", "<<
            data[2]<<", "<<data[3]<<endl;
#endif // DEBUG
            winclient.setStrut(
                winclient.screen().requestStrut(data[0], data[1], data[2], data[3]));
            winclient.screen().updateAvailableWorkspaceArea();
    }
}

Ewmh::WindowState::WindowState(int t_x, int t_y, 
                               unsigned int t_width, 
                               unsigned int t_height,
                               int t_layer, unsigned int t_decor) :
    x(t_x), y(t_y),
    layer(t_layer),
    width(t_width),
    height(t_height),
    decor(t_decor)
{}

Ewmh::WindowState *Ewmh::getState(FluxboxWindow &win) {
    SavedState::iterator it = m_savedstate.find(&win);
    if (it == m_savedstate.end())
        return 0;
    else
        return it->second;
}

void Ewmh::clearState(FluxboxWindow &win) {
    WindowState *state = 0;
    SavedState::iterator it = m_savedstate.find(&win);
    if (it == m_savedstate.end())
        return;

    state = it->second;

    m_savedstate.erase(it);
    delete state;
}

void Ewmh::saveState(FluxboxWindow &win, WindowState *state) {
    m_savedstate[&win] = state;
}
