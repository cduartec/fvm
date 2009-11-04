"""
build configuration utilities
"""
import os, re

# defaults

_config = {
    'Testing': {
        'queue': 'standby',
        'cpus' : '8',
        'nodes': '1',
        'walltime': '1:00:00',
        },
    'lammps': {
        'build': 'openmpi',
        },
    'fvm': {
        'parallel': 'False',
        'version': 'release',
        'compiler': 'gcc',
        },
    'MEMOSA': {
        'Build' : 'True',
        },
}

def config(x,y):
    # special hack for 'env'
    if y == 'env':
        new_env = []
        if  _config.has_key(x):
            for e in _config[x]:
                try:
                    new_env.append('%s=%s' % (re.compile(r'env\[(.*)\]').findall(e)[0], _config[x][e]))
                except IndexError:
                    pass
            # Keep compatibility with old format, for now.
            if  _config[x].has_key(y):            
                new_env.append(_config[x][y])
        return new_env
    try:
        # print "GET %s %s -> %s" % (x, y, _config[x][y])
        return _config[x][y]
    except KeyError:
        return ''


section = ''

def set_section(sec):
    global section
    section = sec
    return True

def set_value(val):
    global section
    # print "SET %s %s" %(section, val)
    if section == '':
        print "Error: No section set."
        return False

    eq = val.find('=')
    if eq < 0:
        return False
    lval = val[:eq]
    rval = val[eq+1:]           
    try: 
        _config[section][lval] = rval
    except KeyError: 
        _config[section] = {lval:rval}
    return True

def read(srcpath, cname):
    global _config

    if cname == '': return False

    filename = os.path.join(srcpath, cname)

    lnum = 0
    f = open(filename, 'r')
    while True:
        line = f.readline()
        lnum = lnum + 1
        if not line:
            break
        line = line.rstrip()
        if line == '' or line[0] in '#;':
            continue
        if line[-1] == ':' and set_section(line[:-1]):
            continue
        if line[0].isspace():
            line = line.lstrip()
            if line[0] in '#;':
                continue
            if set_value(line):
                continue
        print "Cannot parse line %s in %s: %s" % (lnum, filename, line)
        return False
    return  True
        
