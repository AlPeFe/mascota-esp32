// Icono de la bandeja dibujado por código: una cabecita low poly de gato.
// El color indica el estado, así no hace falta ningún archivo de imagen.
use tray_icon::Icon;

#[derive(Clone, Copy)]
pub enum State {
    On,
    Searching,
    Off,
}

const SIZE: u32 = 32;

type Point = (f32, f32);

const HEAD: [Point; 8] = [
    (10.0, 8.0), (22.0, 8.0), (29.0, 14.0), (29.0, 21.0),
    (22.0, 29.0), (10.0, 29.0), (3.0, 21.0), (3.0, 14.0),
];
const EAR_LEFT: [Point; 3] = [(4.0, 15.0), (6.0, 1.0), (15.0, 8.0)];
const EAR_RIGHT: [Point; 3] = [(17.0, 8.0), (26.0, 1.0), (28.0, 15.0)];
const EYE_LEFT: [Point; 4] = [(9.0, 14.0), (13.0, 14.0), (13.0, 20.0), (9.0, 20.0)];
const EYE_RIGHT: [Point; 4] = [(19.0, 14.0), (23.0, 14.0), (23.0, 20.0), (19.0, 20.0)];

// Polígono convexo con los vértices en orden horario (la Y va hacia abajo).
fn inside(polygon: &[Point], x: f32, y: f32) -> bool {
    (0..polygon.len()).all(|i| {
        let (ax, ay) = polygon[i];
        let (bx, by) = polygon[(i + 1) % polygon.len()];
        (bx - ax) * (y - ay) - (by - ay) * (x - ax) >= 0.0
    })
}

pub fn tray_icon(state: State) -> Icon {
    let (fur, ear): ([u8; 3], [u8; 3]) = match state {
        State::On => ([238, 170, 110], [214, 140, 86]),
        State::Searching => ([240, 205, 110], [214, 175, 80]),
        State::Off => ([165, 168, 178], [135, 138, 148]),
    };
    let ink = [34, 30, 46];

    let mut rgba = Vec::with_capacity((SIZE * SIZE * 4) as usize);
    for py in 0..SIZE {
        for px in 0..SIZE {
            let (x, y) = (px as f32 + 0.5, py as f32 + 0.5);
            let color = if inside(&EYE_LEFT, x, y) || inside(&EYE_RIGHT, x, y) {
                Some(ink)
            } else if inside(&HEAD, x, y) {
                Some(fur)
            } else if inside(&EAR_LEFT, x, y) || inside(&EAR_RIGHT, x, y) {
                Some(ear)
            } else {
                None
            };
            match color {
                Some([r, g, b]) => rgba.extend_from_slice(&[r, g, b, 255]),
                None => rgba.extend_from_slice(&[0, 0, 0, 0]),
            }
        }
    }
    Icon::from_rgba(rgba, SIZE, SIZE).expect("icono de la bandeja no valido")
}
