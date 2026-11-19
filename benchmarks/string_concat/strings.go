package main

import (
	"fmt"
	"strings"
)

func main() {
	var sb strings.Builder
	sb.WriteString("start")
	for i := 0; i < 50000; i++ {
		sb.WriteString("a")
	}
	fmt.Println(50000)
}

