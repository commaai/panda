from .helpers import reset_pandas, test_all_pandas, panda_connect_and_init

# Reset the pandas before flashing them
def aaaa_reset_before_tests():
  reset_pandas()

@test_all_pandas
@panda_connect_and_init
def test_recover(p):
  assert p.recover(timeout=30)

@test_all_pandas
@panda_connect_and_init
def test_flash(p):
  p.flash()
