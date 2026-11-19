package main

import "fmt"

func runMatrix(n int) int {
	total := 0
	for i := 0; i < n; i++ {
		for j := 0; j < n; j++ {
			for k := 0; k < n; k++ {
				total += (i*k + k*j)
			}
		}
	}
	return total
}

func main() {
	res := runMatrix(200)
	fmt.Println(res)
}

