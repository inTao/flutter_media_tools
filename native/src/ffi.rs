use std::ffi::{CStr, CString};
use std::os::raw::c_char;

use crate::api;

#[repr(C)]
pub struct NativeResult {
    pub data: *mut u8,
    pub len: usize,
    pub error: *mut c_char,
}

impl NativeResult {
    fn ok(data: Vec<u8>) -> Self {
        let mut boxed = data.into_boxed_slice();
        let ptr = boxed.as_mut_ptr();
        let len = boxed.len();
        std::mem::forget(boxed);
        NativeResult {
            data: ptr,
            len,
            error: std::ptr::null_mut(),
        }
    }

    fn err(msg: String) -> Self {
        NativeResult {
            data: std::ptr::null_mut(),
            len: 0,
            error: CString::new(msg).unwrap_or_default().into_raw(),
        }
    }
}

#[no_mangle]
pub extern "C" fn native_greet(name: *const c_char) -> *mut c_char {
    let name = unsafe { CStr::from_ptr(name) }.to_string_lossy().into_owned();
    CString::new(api::greet(name)).unwrap_or_default().into_raw()
}

#[no_mangle]
pub extern "C" fn native_free_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        unsafe { drop(CString::from_raw(ptr)) };
    }
}

#[no_mangle]
pub extern "C" fn native_free_result(result: NativeResult) {
    if !result.data.is_null() {
        unsafe {
            drop(Vec::from_raw_parts(result.data, result.len, result.len));
        }
    }
    if !result.error.is_null() {
        unsafe { drop(CString::from_raw(result.error)) };
    }
}

#[no_mangle]
pub extern "C" fn native_image_info(data: *const u8, len: usize) -> NativeResult {
    let bytes = unsafe { std::slice::from_raw_parts(data, len) }.to_vec();
    match api::image_info(bytes) {
        Ok(info) => NativeResult::ok(
            format!("{}|{}|{}", info.width, info.height, info.format).into_bytes(),
        ),
        Err(e) => NativeResult::err(e.to_string()),
    }
}

#[no_mangle]
pub extern "C" fn native_resize_image(
    data: *const u8,
    len: usize,
    width: u32,
    height: u32,
) -> NativeResult {
    let bytes = unsafe { std::slice::from_raw_parts(data, len) }.to_vec();
    match api::resize_image(bytes, width, height) {
        Ok(out) => NativeResult::ok(out),
        Err(e) => NativeResult::err(e.to_string()),
    }
}

#[no_mangle]
pub extern "C" fn native_rotate_image(
    data: *const u8,
    len: usize,
    degrees: f32,
) -> NativeResult {
    let bytes = unsafe { std::slice::from_raw_parts(data, len) }.to_vec();
    match api::rotate_image(bytes, degrees) {
        Ok(out) => NativeResult::ok(out),
        Err(e) => NativeResult::err(e.to_string()),
    }
}

#[no_mangle]
pub extern "C" fn native_crop_image(
    data: *const u8,
    len: usize,
    x: u32,
    y: u32,
    width: u32,
    height: u32,
) -> NativeResult {
    let bytes = unsafe { std::slice::from_raw_parts(data, len) }.to_vec();
    match api::crop_image(bytes, x, y, width, height) {
        Ok(out) => NativeResult::ok(out),
        Err(e) => NativeResult::err(e.to_string()),
    }
}

#[no_mangle]
pub extern "C" fn native_blur_image(data: *const u8, len: usize, sigma: f32) -> NativeResult {
    let bytes = unsafe { std::slice::from_raw_parts(data, len) }.to_vec();
    match api::blur_image(bytes, sigma) {
        Ok(out) => NativeResult::ok(out),
        Err(e) => NativeResult::err(e.to_string()),
    }
}

#[no_mangle]
pub extern "C" fn native_grayscale_image(data: *const u8, len: usize) -> NativeResult {
    let bytes = unsafe { std::slice::from_raw_parts(data, len) }.to_vec();
    match api::grayscale_image(bytes) {
        Ok(out) => NativeResult::ok(out),
        Err(e) => NativeResult::err(e.to_string()),
    }
}

#[no_mangle]
pub extern "C" fn native_convert_format(
    data: *const u8,
    len: usize,
    target_format: *const c_char,
) -> NativeResult {
    let bytes = unsafe { std::slice::from_raw_parts(data, len) }.to_vec();
    let fmt = unsafe { CStr::from_ptr(target_format) }
        .to_string_lossy()
        .into_owned();
    match api::convert_format(bytes, fmt) {
        Ok(out) => NativeResult::ok(out),
        Err(e) => NativeResult::err(e.to_string()),
    }
}
