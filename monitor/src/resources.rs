// Lectura de los recursos del PC y su paquete JSON para la placa.
use std::fmt::Write;
use std::path::PathBuf;
use std::time::Instant;

use chrono::Timelike;
use nvml_wrapper::enum_wrappers::device::TemperatureSensor;
use nvml_wrapper::Nvml;
use sysinfo::{CpuRefreshKind, Disks, MemoryRefreshKind, Networks, RefreshKind, System};

const GIB: f64 = 1_073_741_824.0;
const KIB: f64 = 1024.0;
const MAX_CORES: usize = 32;

pub struct Resources {
    system: System,
    disks: Disks,
    networks: Networks,
    nvml: Option<Nvml>,
    system_disk: PathBuf,
    last_sample: Instant,
}

impl Resources {
    pub fn new() -> Result<Self, sysinfo::Error> {
        let system = System::new_with_specifics(
            RefreshKind::nothing()
                .with_cpu(CpuRefreshKind::nothing().with_usage())
                .with_memory(MemoryRefreshKind::nothing().with_ram()),
        )?;
        Ok(Self {
            system,
            disks: Disks::new_with_refreshed_list()?,
            networks: Networks::new_with_refreshed_list()?,
            nvml: Nvml::init().ok(), // sin GPU NVIDIA, la tarjeta sale vacía
            system_disk: system_disk(),
            last_sample: Instant::now(),
        })
    }

    pub fn sample_json(&mut self) -> String {
        let now = Instant::now();
        let seconds = now.duration_since(self.last_sample).as_secs_f64().max(0.001);
        self.last_sample = now;

        self.system.refresh_cpu_usage();
        self.system.refresh_memory_specifics(MemoryRefreshKind::nothing().with_ram());
        self.disks.refresh(true);
        self.networks.refresh(true);

        let cores: Vec<String> = self
            .system
            .cpus()
            .iter()
            .take(MAX_CORES)
            .map(|c| format!("{:.0}", c.usage()))
            .collect();

        let ram_total = self.system.total_memory() as f64;
        let ram_used = self.system.used_memory() as f64;

        let (mut disk_percent, mut read, mut written) = (0.0, 0u64, 0u64);
        for disk in self.disks.iter() {
            let usage = disk.usage();
            read += usage.read_bytes;
            written += usage.written_bytes;
            if disk.mount_point() == self.system_disk && disk.total_space() > 0 {
                let used = disk.total_space() - disk.available_space();
                disk_percent = used as f64 / disk.total_space() as f64 * 100.0;
            }
        }

        let (mut sent, mut received) = (0u64, 0u64);
        for (name, data) in self.networks.iter() {
            let name = name.to_string_lossy().to_lowercase();
            if name == "lo" || name.contains("loopback") {
                continue;
            }
            sent += data.transmitted();
            received += data.received();
        }

        let time = chrono::Local::now();
        let kbps = |bytes: u64| bytes as f64 / seconds / KIB;

        let mut out = String::with_capacity(320);
        let _ = write!(
            out,
            "{{\"t\":\"res\",\"cpu\":{:.1},\"cores\":[{}],\"ram\":{:.1},\"ramU\":{:.1},\"ramT\":{:.1},\
             \"disk\":{:.1},\"dr\":{:.0},\"dw\":{:.0},\"up\":{:.0},\"dn\":{:.0},\"hh\":{},\"mm\":{}",
            self.system.global_cpu_usage(),
            cores.join(","),
            ram_used / ram_total.max(1.0) * 100.0,
            ram_used / GIB,
            ram_total / GIB,
            disk_percent,
            kbps(read),
            kbps(written),
            kbps(sent),
            kbps(received),
            time.hour(),
            time.minute(),
        );
        self.write_gpu(&mut out);
        if let Some(idle) = idle_seconds() {
            let _ = write!(out, ",\"idle\":{idle}");
        }
        out.push_str("}\n");
        out
    }

    fn write_gpu(&self, out: &mut String) {
        let device = self.nvml.as_ref().and_then(|n| n.device_by_index(0).ok());
        let reading = device.as_ref().and_then(|d| {
            Some((d.utilization_rates().ok()?, d.memory_info().ok()?, d.name().ok()?))
        });
        let Some((usage, memory, name)) = reading else {
            out.push_str(",\"gpu\":-1,\"vram\":-1");
            return;
        };
        let _ = write!(
            out,
            ",\"gpu\":{},\"vram\":{:.1},\"vramU\":{:.1},\"vramT\":{:.1},\"gpuN\":\"{}\"",
            usage.gpu,
            memory.used as f64 / memory.total.max(1) as f64 * 100.0,
            memory.used as f64 / GIB,
            memory.total as f64 / GIB,
            short_gpu_name(&name),
        );
        if let Some(temp) = device.as_ref().and_then(|d| d.temperature(TemperatureSensor::Gpu).ok()) {
            let _ = write!(out, ",\"gpuT\":{temp}");
        }
    }
}

// Segundos desde la última pulsación o movimiento de ratón. Es lo único que se
// mira de la actividad: el sistema da la hora del último evento, no su contenido.
#[cfg(windows)]
fn idle_seconds() -> Option<u32> {
    use windows_sys::Win32::System::SystemInformation::GetTickCount;
    use windows_sys::Win32::UI::Input::KeyboardAndMouse::{GetLastInputInfo, LASTINPUTINFO};

    let mut info = LASTINPUTINFO { cbSize: std::mem::size_of::<LASTINPUTINFO>() as u32, dwTime: 0 };
    // SAFETY: info es válida y cbSize tiene el tamaño que pide la función.
    if unsafe { GetLastInputInfo(&mut info) } == 0 {
        return None;
    }
    // SAFETY: GetTickCount no tiene requisitos.
    let now = unsafe { GetTickCount() };
    Some(now.wrapping_sub(info.dwTime) / 1000)
}

// En Linux no hay una forma común a X11 y Wayland: la placa usa entonces la carga del PC.
#[cfg(not(windows))]
fn idle_seconds() -> Option<u32> {
    None
}

fn system_disk() -> PathBuf {
    if cfg!(windows) {
        let drive = std::env::var("SystemDrive").unwrap_or_else(|_| "C:".into());
        PathBuf::from(format!("{drive}\\"))
    } else {
        PathBuf::from("/")
    }
}

// "NVIDIA GeForce RTX 5070 Ti" -> "RTX 5070 Ti"; solo caracteres seguros para el JSON
fn short_gpu_name(name: &str) -> String {
    name.replace("NVIDIA ", "")
        .replace("GeForce ", "")
        .chars()
        .filter(|c| c.is_ascii_alphanumeric() || *c == ' ' || *c == '-')
        .take(23)
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sample_has_every_field() {
        let mut resources = Resources::new().unwrap();
        std::thread::sleep(std::time::Duration::from_millis(1200));
        let line = resources.sample_json();
        println!("{line}");
        for key in ["cpu", "cores", "ram", "ramU", "ramT", "disk", "dr", "dw", "up", "dn", "hh", "mm", "gpu", "vram"] {
            assert!(line.contains(&format!("\"{key}\":")), "falta {key}");
        }
        assert!(line.ends_with("}\n"));
    }

    #[test]
    fn gpu_name_is_short_and_safe() {
        assert_eq!(short_gpu_name("NVIDIA GeForce RTX 5070 Ti"), "RTX 5070 Ti");
        assert_eq!(short_gpu_name("Bad\"Name"), "BadName");
    }
}
