//go:build ignore

package example

import (
	"fmt"
)

import "io"

func main() {
	fmt.Println(0)
}

func foo(io.Reader) {
	fmt.Println(0)
}

type C chan int

type T struct {
	f0     int
	f1, f2 [3]float32
	f6     struct {
		x    int
		y, z string
	}
	f7 struct{ x int }
}

func (x T) aoo() {
	fmt.Println(1)
}

func (T) boo() {
	fmt.Println(1.0)
}

func (g **C) coo() {
	fmt.Println("one")
}

type element[T any] struct {
	next *element[T]
	val  T
}

func moo()

var x, y, z int

var abc = ghi(j,
	k)

const (
	c1     = 123.5
	c2, c3 = "k2", '?'
)

var (
	v6, v7 = 8, 9.0e+1

	someStruct = struct{ x int }
)

const space string = " "

const (
	A     = "U+0041 LATIN CAPITAL LETTER A"
	À     = "U+00C0 LATIN CAPITAL LETTER A WITH GRAVE"
	α12_z = "U+03B1 GREEK SMALL LETTER ALPHA, 1, 2, _, z"
	яж    = "U+044F CYRILLIC SMALL LETTER YA, U+0436 CYRILLIC SMALL LETTER ZHE"

	// This is a valid Go variable name (it's a Unicode letter) but, for
	// implementation simplicity, clloverview's is_namey_rune ignores it. This
	// is arguably a false negative, but cases like these are rare in practice.
	ﬁ = "U+FB01 LATIN SMALL LIGATURE FI"
)
