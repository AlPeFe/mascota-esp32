// Hilo que busca la placa y le envía los datos. Apagado se queda dormido, sin
// puerto abierto ni lecturas del sistema.
use std::io::{Read, Write};
use std::sync::{Condvar, Mutex};
use std::time::{Duration, Instant};

use serialport::{ClearBuffer, SerialPort, SerialPortType};

use crate::resources::Resources;

const BAUD_RATE: u32 = 115_200;
const GREETING: &str = "MASCOTA_ESP32";
const SEND_INTERVAL: Duration = Duration::from_secs(1);
const SEARCH_INTERVAL: Duration = Duration::from_secs(3);
const HANDSHAKE_TIMEOUT: Duration = Duration::from_secs(6);
// Chips USB-serie habituales en placas ESP32: CH340, CP210x, FTDI y Espressif
const KNOWN_VIDS: [u16; 4] = [0x1A86, 0x10C4, 0x0403, 0x303A];

pub enum Status {
    Off,
    Searching,
    Connected(String),
}

pub struct Control {
    enabled: Mutex<bool>,
    changed: Condvar,
}

impl Control {
    pub fn new(enabled: bool) -> Self {
        Self { enabled: Mutex::new(enabled), changed: Condvar::new() }
    }

    pub fn set_enabled(&self, value: bool) {
        *self.enabled.lock().unwrap() = value;
        self.changed.notify_all();
    }

    fn is_enabled(&self) -> bool {
        *self.enabled.lock().unwrap()
    }

    fn wait_until_enabled(&self) {
        let mut enabled = self.enabled.lock().unwrap();
        while !*enabled {
            enabled = self.changed.wait(enabled).unwrap();
        }
    }

    // Espera el tiempo indicado. Devuelve false si lo apagan mientras tanto.
    fn sleep_while_enabled(&self, duration: Duration) -> bool {
        let enabled = self.enabled.lock().unwrap();
        let (enabled, _) = self.changed.wait_timeout_while(enabled, duration, |on| *on).unwrap();
        *enabled
    }
}

pub fn run(control: &Control, notify: impl Fn(Status)) {
    loop {
        if !control.is_enabled() {
            notify(Status::Off);
            control.wait_until_enabled();
        }
        notify(Status::Searching);

        let Some((mut port, name)) = find_board(control) else {
            control.sleep_while_enabled(SEARCH_INTERVAL);
            continue;
        };
        notify(Status::Connected(name));

        let Ok(mut resources) = Resources::new() else {
            control.sleep_while_enabled(SEARCH_INTERVAL);
            continue;
        };
        while control.sleep_while_enabled(SEND_INTERVAL) {
            let line = resources.sample_json();
            if port.write_all(line.as_bytes()).is_err() {
                break; // la placa se ha desconectado
            }
            let _ = port.clear(ClearBuffer::Input);
        }
        // Aquí se sueltan el puerto y las lecturas del sistema
    }
}

fn find_board(control: &Control) -> Option<(Box<dyn SerialPort>, String)> {
    let ports = serialport::available_ports().ok()?;
    for info in ports {
        let SerialPortType::UsbPort(usb) = &info.port_type else { continue };
        if !KNOWN_VIDS.contains(&usb.vid) {
            continue;
        }
        if !control.is_enabled() {
            return None;
        }
        if let Some(port) = greet(&info.port_name, control) {
            return Some((port, info.port_name));
        }
    }
    None
}

// Abre el puerto y pregunta "?" hasta que la placa responde con su saludo.
fn greet(name: &str, control: &Control) -> Option<Box<dyn SerialPort>> {
    let mut port = serialport::new(name, BAUD_RATE)
        .timeout(Duration::from_millis(200))
        .dtr_on_open(false) // sin esto la placa se reinicia al abrir el puerto
        .open()
        .ok()?;
    let _ = port.write_request_to_send(false);

    let deadline = Instant::now() + HANDSHAKE_TIMEOUT;
    let mut received = Vec::new();
    let mut chunk = [0u8; 64];
    while Instant::now() < deadline && control.is_enabled() {
        port.write_all(b"?\n").ok()?;
        if let Ok(n) = port.read(&mut chunk) {
            received.extend_from_slice(&chunk[..n]);
            if String::from_utf8_lossy(&received).contains(GREETING) {
                return Some(port);
            }
            if received.len() > 1024 {
                received.clear();
            }
        }
    }
    None
}
