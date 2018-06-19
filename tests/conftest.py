


def pytest_generate_tests(metafunc):
    if 'ksize' in metafunc.fixturenames:
        ksize = getattr(metafunc.function, '_ksize', None)
        if ksize is None:
            ksize = [21]
        if isinstance(ksize, int):
            ksize = [ksize]
        metafunc.parametrize('ksize', ksize,  
                             ids=lambda k: 'K={0}'.format(k))


