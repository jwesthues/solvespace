from unittest import defaultTestLoader, TextTestRunner

if __name__ == '__main__':
    TextTestRunner().run(defaultTestLoader.discover('test'))
