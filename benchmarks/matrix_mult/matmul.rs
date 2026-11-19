fn run_matrix(n: i32) -> i32 {
    let mut total: i32 = 0;
    for i in 0..n {
        for j in 0..n {
            for k in 0..n {
                total = total.wrapping_add(i * k + k * j);
            }
        }
    }
    total
}

fn main() {
    let res = run_matrix(200);
    println!("{}", res);
}

