// RUN: {self}
// RET: 255
// TXT: {self}:5: u[3][2][1][-4] = {[2][1]=1};
// TXT:                                                ^ error: array declared with negative bounds
u[3][2][1][-4] = {[2][1]=1};
