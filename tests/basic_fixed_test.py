#! /usr/bin/python3
import os
import random


NORMAL  = '\033[39m'
OKGREEN = '\033[92m'
BADRED  = '\033[91m'


def run_it(cmd):
    with os.popen("./basic_fixed " + cmd) as f:
        return f.readline().strip()


def test():
    u = (random.randint(1, 10000), 0, -random.randint(1, 10000))
    l = (random.randint(1, 1000000), 0)
    o = "-+/*"

    good = 0

    r = run_it("500")
    if r != "500":
        print(OKGREEN + "Integer" + NORMAL)
    else:
        print(BADRED + "Integer" + NORMAL)

    r = run_it("10.10000001")
    if r == "10.100000":
        print(OKGREEN + "Float over long" + NORMAL)
    else:
        print(BADRED + "Float over long" + NORMAL)

    r = run_it("10.1")
    if r == "10.100000":
        print(OKGREEN + "Float short" + NORMAL)
    else:
        print(BADRED + "Float short" + NORMAL)

    for n in range(0, len(o)):
      for i in range(0, len(u)):
        for j in range(0, len(l)):
          for x in range(0, len(u)):
            for y in range(0, len(l)):
              a = "%d.%06u" % (u[i], l[j])
              b = "%d.%06u" % (u[x], l[y])
              op = o[n]
              if op != '/' or b != "0.000000":
                r = run_it("%s \\%s %s" % (a,op,b))
                r_n = float(r)
                s = "%s %s %s" % (a,op,b)
                r_n2 = eval(s)
                if r_n and r_n2:
                    e = abs(r_n / r_n2 * 100)
                else:
                    e = 100
                status = ""
                if e > 99 and e < 99.5:
                    status = OKGREEN + "<<<< OK" + NORMAL
                elif e < 95:
                    if r.find("0.00000") != -1:
                        status = OKGREEN + "<<<< SMALL OK " + NORMAL
                    else:
                        status = BADRED + "<<<< BAD" + NORMAL
                        if e < 90:
                            status = BADRED + "<<<< TERRIBLE" + NORMAL
                            assert e > 70, s

                if status:
                    print(s, "=", r, ",", r_n2, ": %G%%" % e, status, "After %u Good" % good)
                else:
                    good += 1

    print("Done after %u Good" % good)


test()
