import os
import tempfile
import time

SYMBOLIC_FLAGS = ['O_RDONLY', 'O_EXEC', 'O_SEARCH', 'O_PATH']

NUMERIC_FLAGS = {
  '0x40000 (freebsd O_EXEC/O_SEARCH/O_PATH)': 0x40000, # https://cgit.freebsd.org/src/tree/sys/sys/fcntl.h#n121
}

DEBUG = False


def try_read(dirfds):
  for flag, dirfd in dirfds.items():
    try:
      fd = os.open('test', os.O_RDONLY, dir_fd=dirfd)
      result = os.read(fd, 5)
      if result != b'hello':
        raise ValueError(f'Read {result}, want "hello"')
      os.close(fd)
      print(f'Successful read using dirfd opened with {flag}')
    except Exception as e:
      print(f'Failed to read using dirfd opened with {flag}: {e}')



with tempfile.TemporaryDirectory() as tempdir:
  os.chdir(tempdir)
  os.mkdir('private-dir')

  dirfds = {}

  for f in SYMBOLIC_FLAGS:
    if f not in os.__dict__:
      print(f'os.{f} not defined')
      continue
    try:
      dirfds[f] = os.open('private-dir', os.__dict__[f])
      print(f'Opened dir with flag {f}')
    except Exception as e:
      print(f"Failed to open dir with flag {f}: {e}")

  for f, n in NUMERIC_FLAGS.items():
    try:
      dirfds[f] = os.open('private-dir', n)
      print(f'Opened dir with flag {f}')
    except Exception as e:
      print(f"Failed to open dir with flag {f}: {e}")


  with open('private-dir/test', 'w+') as testfile:
    testfile.write('hello')

  print('\n=== BEFORE chmod: ===')
  try_read(dirfds)
  os.chmod('private-dir', 000)
  print('\n=== AFTER chmod: ===')
  try_read(dirfds)

  print(f'\nSleeping so you can examine {tempdir} (CTRL-C to cleanup and quit)...')
  time.sleep(24*3600)

