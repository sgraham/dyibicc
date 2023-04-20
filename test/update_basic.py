from test_helpers_for_update import *

SRC = '''\
int main(void) {
  return 0;
}
'''

initial({'main.c': SRC})
update_ok()
expect(0)

sub('main.c', 2, '0', '1')
update_ok()
expect(1)

sub('main.c', 2, '1', '99')
update_ok()
expect(99)

done()
