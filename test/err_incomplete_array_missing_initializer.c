// RUN: {self}
// RET: 255
// TXT: {self}:5: int A[][] = ;
// TXT:                                                        ^ error: array has incomplete element type
int A[][] = ;
