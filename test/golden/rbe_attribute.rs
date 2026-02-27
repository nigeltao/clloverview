// https://doc.rust-lang.org/rust-by-example/attribute.html

#[derive(Debug)]
struct Rectangle {
    width: u32,
    height: u32,
}

#![allow(unused_variables)]

fn main() {
    let x = 3; // This would normally warn about an unused variable.
}
