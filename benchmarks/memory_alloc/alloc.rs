struct Node {
    value: i32,
}

fn main() {
    let mut sum: i32 = 0;
    for i in 0..1000000 {
        let b = Box::new(Node { value: i });
        sum = sum.wrapping_add(b.value);
    }
    println!("{}", sum);
}

