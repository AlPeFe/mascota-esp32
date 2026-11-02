// Monitor de la mascota: un icono en la bandeja del sistema (Windows o Linux)
// que envía los recursos del PC a la placa por USB. Desde su menú se puede
// apagar, y entonces no lee nada ni ocupa el puerto.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod icon;
mod resources;
mod worker;

use std::net::TcpListener;
use std::sync::Arc;

use auto_launch::{AutoLaunch, AutoLaunchBuilder};
use tao::event::{Event, StartCause};
use tao::event_loop::{ControlFlow, EventLoopBuilder};
use tray_icon::menu::{CheckMenuItem, Menu, MenuEvent, MenuItem, PredefinedMenuItem};
use tray_icon::{TrayIcon, TrayIconBuilder};

use worker::{Control, Status};

enum UserEvent {
    Menu(MenuEvent),
    Status(Status),
}

struct Tray {
    _icon: TrayIcon,
    handle: TrayHandle,
}

struct TrayHandle {
    status: MenuItem,
    enabled: CheckMenuItem,
    autostart: CheckMenuItem,
    quit: MenuItem,
}

// Un puerto local como cerrojo: impide abrir el monitor dos veces, porque dos
// copias se pelearían por el puerto serie. El sistema lo libera al salir.
const INSTANCE_LOCK: &str = "127.0.0.1:47219";

fn autostart_entry() -> Option<AutoLaunch> {
    let exe = std::env::current_exe().ok()?;
    AutoLaunchBuilder::new()
        .set_app_name("Mascota Monitor")
        .set_app_path(exe.to_str()?)
        .build()
        .ok()
}

fn build_tray(control_on: bool, autostart_on: bool) -> Tray {
    let status = MenuItem::new("Buscando la placa...", false, None);
    let enabled = CheckMenuItem::new("Monitor encendido", true, control_on, None);
    let autostart = CheckMenuItem::new("Arrancar con el sistema", true, autostart_on, None);
    let quit = MenuItem::new("Salir", true, None);

    let menu = Menu::new();
    menu.append_items(&[
        &status,
        &PredefinedMenuItem::separator(),
        &enabled,
        &autostart,
        &PredefinedMenuItem::separator(),
        &quit,
    ])
    .expect("no se pudo crear el menu");

    let tray = TrayIconBuilder::new()
        .with_menu(Box::new(menu))
        .with_tooltip("Mascota: buscando la placa")
        .with_icon(icon::tray_icon(icon::State::Searching))
        .build()
        .expect("no se pudo crear el icono de la bandeja");

    Tray {
        _icon: tray,
        handle: TrayHandle { status, enabled, autostart, quit },
    }
}

fn show_status(tray: &Tray, status: &Status) {
    let (text, state) = match status {
        Status::Off => ("Apagado".to_string(), icon::State::Off),
        Status::Searching => ("Buscando la placa...".to_string(), icon::State::Searching),
        Status::Connected(port) => (format!("Conectado en {port}"), icon::State::On),
    };
    tray.handle.status.set_text(&text);
    let _ = tray._icon.set_tooltip(Some(format!("Mascota: {text}")));
    let _ = tray._icon.set_icon(Some(icon::tray_icon(state)));
}

fn main() {
    let Ok(_lock) = TcpListener::bind(INSTANCE_LOCK) else {
        return; // ya hay otro monitor funcionando
    };

    // Modo sin bandeja (--sin-bandeja): para servidores, SSH o cuando no hay
    // escritorio. Solo envía los datos; se para con Ctrl+C o con systemd.
    if std::env::args().any(|a| a == "--sin-bandeja") {
        let on = !std::env::args().any(|a| a == "--apagado");
        let control = Control::new(on);
        eprintln!("mascota-monitor: sin bandeja, buscando la placa por USB");
        worker::run(&control, |status| match status {
            Status::Off => eprintln!("mascota-monitor: apagado"),
            Status::Searching => eprintln!("mascota-monitor: buscando la placa..."),
            Status::Connected(port) => eprintln!("mascota-monitor: conectado en {port}"),
        });
        return;
    }

    let event_loop = EventLoopBuilder::<UserEvent>::with_user_event().build();

    let proxy = event_loop.create_proxy();
    MenuEvent::set_event_handler(Some(move |event| {
        let _ = proxy.send_event(UserEvent::Menu(event));
    }));

    let start_on = !std::env::args().any(|a| a == "--apagado");
    let control = Arc::new(Control::new(start_on));
    {
        let control = control.clone();
        let proxy = event_loop.create_proxy();
        std::thread::spawn(move || {
            worker::run(&control, |status| {
                let _ = proxy.send_event(UserEvent::Status(status));
            })
        });
    }

    let autostart = autostart_entry();
    let mut tray: Option<Tray> = None;

    // ControlFlow::Wait: el hilo duerme hasta que llega un evento
    event_loop.run(move |event, _, flow| {
        *flow = ControlFlow::Wait;
        match event {
            Event::NewEvents(StartCause::Init) => {
                let on = autostart.as_ref().and_then(|a| a.is_enabled().ok()).unwrap_or(false);
                tray = Some(build_tray(start_on, on));
            }
            Event::UserEvent(UserEvent::Status(status)) => {
                if let Some(tray) = &tray {
                    show_status(tray, &status);
                }
            }
            Event::UserEvent(UserEvent::Menu(event)) => {
                let Some(tray) = &tray else { return };
                let h = &tray.handle;
                if event.id == *h.enabled.id() {
                    control.set_enabled(h.enabled.is_checked());
                } else if event.id == *h.autostart.id() {
                    if let Some(a) = &autostart {
                        let result = if h.autostart.is_checked() { a.enable() } else { a.disable() };
                        if result.is_err() {
                            h.autostart.set_checked(!h.autostart.is_checked());
                        }
                    }
                } else if event.id == *h.quit.id() {
                    *flow = ControlFlow::Exit;
                }
            }
            _ => {}
        }
    });
}
