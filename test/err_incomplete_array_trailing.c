// RUN: {self}
// RET: 255
// TXT: {self}:5: _[4u][];
// TXT:                                         ^ error: incomplete type for array
_[4u][];
