#include <windows.h>
#include "debug.h"
#include "winapi_hooks.h"
#include "dd.h"
#include "hook.h"


HHOOK g_mouse_hook;
HOOKPROC g_mouse_proc;

void mouse_lock()
{
    if (g_ddraw->devmode || g_ddraw->bnet_active)
        return;

    if (g_hook_active && !g_ddraw->locked)
    {
        RECT rc = {
            g_ddraw->render.viewport.x,
            g_ddraw->render.viewport.y,
            g_ddraw->width + g_ddraw->render.viewport.x,
            g_ddraw->height + g_ddraw->render.viewport.y,
        };

        if (g_ddraw->adjmouse)
        {
            rc.right = g_ddraw->render.viewport.width + g_ddraw->render.viewport.x;
            rc.bottom = g_ddraw->render.viewport.height + g_ddraw->render.viewport.y;
        }

        real_MapWindowPoints(g_ddraw->hwnd, HWND_DESKTOP, (LPPOINT)&rc, 2);

        rc.bottom -= (LONG)((g_ddraw->mouse_y_adjust * 2) * g_ddraw->render.scale_h);

        int cur_x = InterlockedExchangeAdd((LONG*)&g_ddraw->cursor.x, 0);
        int cur_y = InterlockedExchangeAdd((LONG*)&g_ddraw->cursor.y, 0);

        if (g_ddraw->adjmouse)
        {
            real_SetCursorPos(
                (int)(rc.left + (cur_x * g_ddraw->render.scale_w)),
                (int)(rc.top + ((cur_y - g_ddraw->mouse_y_adjust) * g_ddraw->render.scale_h)));
        }
        else
        {
            real_SetCursorPos(rc.left + cur_x, rc.top + cur_y - g_ddraw->mouse_y_adjust);
        }

        int game_count = InterlockedExchangeAdd((LONG*)&g_ddraw->show_cursor_count, 0);
        int cur_count = real_ShowCursor(TRUE) - 1;
        real_ShowCursor(FALSE);

        if (cur_count > game_count)
        {
            while (real_ShowCursor(FALSE) > game_count);
        }
        else if (cur_count < game_count)
        {
            while (real_ShowCursor(TRUE) < game_count);
        }

        real_SetCursor((HCURSOR)InterlockedExchangeAdd((LONG*)&g_ddraw->old_cursor, 0));

        real_ClipCursor(&rc);

        g_ddraw->locked = TRUE;
    }
}

void mouse_unlock()
{
    if (g_ddraw->devmode || !g_hook_active)
        return;

    if (g_ddraw->locked)
    {
        g_ddraw->locked = FALSE;

        RECT rc;
        real_GetClientRect(g_ddraw->hwnd, &rc);
        real_MapWindowPoints(g_ddraw->hwnd, HWND_DESKTOP, (LPPOINT)&rc, 2);

        CURSORINFO ci = { .cbSize = sizeof(CURSORINFO) };
        if (real_GetCursorInfo(&ci) && ci.flags == 0)
        {
            while (real_ShowCursor(TRUE) < 0);
        }

        real_ClipCursor(NULL);

        int cur_x = InterlockedExchangeAdd((LONG*)&g_ddraw->cursor.x, 0);
        int cur_y = InterlockedExchangeAdd((LONG*)&g_ddraw->cursor.y, 0);

        real_SetCursorPos(
            (int)(rc.left + g_ddraw->render.viewport.x + (cur_x * g_ddraw->render.scale_w)),
            (int)(rc.top + g_ddraw->render.viewport.y + ((cur_y + g_ddraw->mouse_y_adjust) * g_ddraw->render.scale_h)));
    }
}

LRESULT CALLBACK mouse_hook_proc(int Code, WPARAM wParam, LPARAM lParam)
{
    if (!g_ddraw || !g_ddraw->fixmousehook)
        return g_mouse_proc(Code, wParam, lParam);

    if (Code < 0)
        return CallNextHookEx(g_mouse_hook, Code, wParam, lParam);

    switch (wParam)
    {
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        if (!g_ddraw->devmode && !g_ddraw->locked)
        {
            mouse_lock();
            return CallNextHookEx(g_mouse_hook, Code, wParam, lParam);
        }
        break;
    }
    /* down messages are ignored if we have no cursor lock */
    case WM_XBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHOVER:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_MOUSEMOVE:
    {
        if (!g_ddraw->devmode && !g_ddraw->locked)
        {
            return CallNextHookEx(g_mouse_hook, Code, wParam, lParam);
        }
        break;
    }
    }

    fake_GetCursorPos(&((MOUSEHOOKSTRUCT*)lParam)->pt);

    return g_mouse_proc(Code, wParam, lParam);
}
