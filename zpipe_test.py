#!/usr/bin/python3


from zpipe import ZPipe, Zephyrgram


def main():
    zp = ZPipe(['./zpipe'], print)
    # zp = ZPipe(['bash', '-c', 'hd 1>&2'], print)
    zp.subscribe('zpipe-example', 'example')
    for i in range(3):
        text = input('enter a message ({}/3) > '.format(i + 1))
        zp.zwrite(Zephyrgram(None, 'zpipe-example', 'example', None,
                             'zpipe-example', True,
                             ['zpipe example {}/3'.format(i + 1), text]))
    # import time; time.sleep(1)
    zp.close_zephyr()
    return 0

if __name__ == '__main__':
    main()
