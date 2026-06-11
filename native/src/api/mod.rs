use image::ImageFormat;
use std::io::Cursor;

pub fn greet(name: String) -> String {
    format!("Hello, {name}! Media tools Rust core is running.")
}

pub fn image_info(data: Vec<u8>) -> anyhow::Result<ImageInfo> {
    let reader = image::ImageReader::new(Cursor::new(&data)).with_guessed_format()?;
    let format = reader.format();
    let decoded = reader.decode()?;
    Ok(ImageInfo {
        width: decoded.width(),
        height: decoded.height(),
        format: format.map(|f| format!("{:?}", f)).unwrap_or_default(),
    })
}

pub fn resize_image(
    data: Vec<u8>,
    width: u32,
    height: u32,
) -> anyhow::Result<Vec<u8>> {
    let img = image::load_from_memory(&data)?;
    let resized = img.resize_exact(width, height, image::imageops::FilterType::Lanczos3);
    let mut buf = Cursor::new(Vec::new());
    resized.write_to(&mut buf, ImageFormat::Png)?;
    Ok(buf.into_inner())
}

pub fn rotate_image(data: Vec<u8>, degrees: f32) -> anyhow::Result<Vec<u8>> {
    let img = image::load_from_memory(&data)?;
    let rotated = if (degrees - 90.0).abs() < f32::EPSILON {
        img.rotate90()
    } else if (degrees - 180.0).abs() < f32::EPSILON {
        img.rotate180()
    } else if (degrees - 270.0).abs() < f32::EPSILON {
        img.rotate270()
    } else {
        anyhow::bail!("Only 90/180/270 degree rotations are supported");
    };
    let mut buf = Cursor::new(Vec::new());
    rotated.write_to(&mut buf, ImageFormat::Png)?;
    Ok(buf.into_inner())
}

pub fn crop_image(
    data: Vec<u8>,
    x: u32,
    y: u32,
    width: u32,
    height: u32,
) -> anyhow::Result<Vec<u8>> {
    let img = image::load_from_memory(&data)?;
    let cropped = img.crop_imm(x, y, width, height);
    let mut buf = Cursor::new(Vec::new());
    cropped.write_to(&mut buf, ImageFormat::Png)?;
    Ok(buf.into_inner())
}

pub fn blur_image(data: Vec<u8>, sigma: f32) -> anyhow::Result<Vec<u8>> {
    let img = image::load_from_memory(&data)?;
    let blurred = img.blur(sigma);
    let mut buf = Cursor::new(Vec::new());
    blurred.write_to(&mut buf, ImageFormat::Png)?;
    Ok(buf.into_inner())
}

pub fn grayscale_image(data: Vec<u8>) -> anyhow::Result<Vec<u8>> {
    let img = image::load_from_memory(&data)?;
    let gray = img.grayscale();
    let mut buf = Cursor::new(Vec::new());
    gray.write_to(&mut buf, ImageFormat::Png)?;
    Ok(buf.into_inner())
}

pub fn convert_format(data: Vec<u8>, target_format: String) -> anyhow::Result<Vec<u8>> {
    let img = image::load_from_memory(&data)?;
    let fmt = match target_format.to_lowercase().as_str() {
        "png" => ImageFormat::Png,
        "jpeg" | "jpg" => ImageFormat::Jpeg,
        "webp" => ImageFormat::WebP,
        "bmp" => ImageFormat::Bmp,
        _ => anyhow::bail!("Unsupported target format: {}", target_format),
    };
    let mut buf = Cursor::new(Vec::new());
    img.write_to(&mut buf, fmt)?;
    Ok(buf.into_inner())
}

pub struct ImageInfo {
    pub width: u32,
    pub height: u32,
    pub format: String,
}
