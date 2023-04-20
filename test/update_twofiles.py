from test_helpers_for_update import *

SRC1 = '''\
extern int other(void);
int main(void) {
  return other();
}
'''

SRC2 = '''\
int other(void) {
  return 100;
}
'''

initial({'main.c': SRC1, 'second.c': SRC2})
update_ok()
expect(100)

# Update only the second file; main needs to relink against the import.
sub('second.c', 2, '100', '99')
update_ok()
expect(99)

done()
