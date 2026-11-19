package main

import "fmt"

type Node struct {
	value int
}

func main() {
	sum := 0
	for i := 0; i < 1000000; i++ {
		b := new(Node)
		b.value = i
		sum += b.value
	}
	fmt.Println(sum)
}

