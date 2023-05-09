// RUN: {self}
// RET: 255
// TXT: {self}:5: V[10]={[1000000000000000000000]=1};
// TXT:                                                                    ^ error: array designator index exceeds array bounds
V[10]={[1000000000000000000000]=1};
