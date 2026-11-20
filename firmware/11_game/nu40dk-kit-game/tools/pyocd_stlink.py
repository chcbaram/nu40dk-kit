# pyOCD user script: ST-LINK 로 nRF52840 을 굽기 위한 설정
#
# ST-LINK 펌웨어는 nRF52 의 CTRL-AP(AP#1) 접근을 거부한다 ("STLink error (29): Bad AP").
# 그래서 AHB-AP(AP#0) 만 사용하고, pyOCD nrf52 타겟이 discovery 안에 넣는
# CTRL-AP 관련 단계(check_ctrl_ap_idr, check_flash_security, persist_unlock)를 뺀다.
# MEM-AP 로 Flash 쓰기는 정상 동작한다.
# 주의: APPROTECT 가 걸린 칩의 복구(mass erase)는 CTRL-AP 가 필요해서 이 방법으로는 불가능하다.
#
# 사용: pyocd flash -t nrf52840 --script tools/pyocd_stlink.py build/merged.hex

_SKIP = ('check_ctrl_ap_idr', 'check_flash_security', 'persist_unlock')


def _strip(seq):
    for name in _SKIP:
        if seq.has_task(name):
            seq.remove_task(name)
    return seq


def will_init_target(target, init_sequence):
    target.dp.valid_aps = [0]
    # nrf52 타겟은 discovery 를 두 번 wrap 하므로, 그 뒤에 한 번 더 wrap 해서 단계를 제거한다.
    init_sequence.wrap_task('discovery', _strip)
