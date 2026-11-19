fn main() {
    let mut s = String::from("start");
    for _ in 0..50000 {
        s.push_str("a");
    }
    println!("{}", 50000);
}

