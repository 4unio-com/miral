#! /usr/bin/python
from xml.dom import minidom
from sys import argv

helptext = \
"""This script processes the XML generated by "make doc" and produces summary information
on symbols that libmiral intends to make public.

To use:

1. Go to your build folder and run "make doc"
2. Create symbols.map by running "../scripts/process_doxygen_xml.py doc/xml/*.xml > ../miral/symbols.map\""""

debug = False

def get_text(node):
    rc = []
    for node in node.childNodes:
        if node.nodeType == node.TEXT_NODE:
            rc.append(node.data)
        elif node.nodeType == node.ELEMENT_NODE:
            rc.append(get_text(node))
    return ''.join(rc)

def get_text_for_element(parent, tagname):
    rc = []
    nodes = parent.getElementsByTagName(tagname);
    for node in nodes : rc.append(get_text(node))
    return ''.join(rc)

def get_file_location(node):
    for node in node.childNodes:
        if node.nodeType == node.ELEMENT_NODE and node.tagName == 'location':
            return node.attributes['file'].value
    if debug: print 'no location in:', node
    return None
    
def has_element(node, tagname):
    for node in node.childNodes:
        if node.nodeType == node.ELEMENT_NODE and node.tagName in tagname:
            return True
    return False

def print_attribs(node, attribs):
    for attrib in attribs : print ' ', attrib, '=', node.attributes[attrib].value

def concat_text_from_tags(parent, tagnames):
    rc = []
    for tag in tagnames : rc.append(get_text_for_element(parent, tag))
    return ''.join(rc)
    
def print_location(node):
    print ' ', 'location', '=', get_file_location(node)

def get_attribs(node):
    kind = node.attributes['kind'].value
    static = node.attributes['static'].value
    prot =  node.attributes['prot'].value
    return (kind, static, prot)

# Special cases for publishing anyway:
publish_special_cases = {}

component_map = {}
symbols = {'public' : set(), 'private' : set()}

def report(publish, symbol):
    symbol = symbol.replace('~', '?')

    if symbol in publish_special_cases: publish = True

    if publish: symbols['public'].add(symbol)
    else:       symbols['private'].add(symbol)

    if not debug: return
    if publish: print '  PUBLISH: {}'.format(symbol)
    else      : print 'NOPUBLISH: {}'.format(symbol)

def print_report():
    print 'MIRAL_0.1 {'
    print 'global:'
    print '  extern "C++" {'
    for symbol in sorted(symbols['public']): print '    {};'.format(symbol)
    print '  };'
    print 'local: *;'
    print '};'

def print_debug_info(node, attributes):
    if not debug: return
    print
    print_attribs(node, attributes)
    print_location(node)

def find_physical_component(location_file):
    path_elements = location_file.split('/')
    found = False
    for element in path_elements:
        if found: return element
        found = element in ['include', 'src']
    if debug: print 'no component in:', location_file
    return None
    
def parse_member_def(context_name, node, is_class):
    (kind, static, prot) = get_attribs(node)
    
    if kind in ['enum', 'typedef']: return
    if has_element(node, ['templateparamlist']): return
    if kind in ['function'] and node.attributes['inline'].value == 'yes': return
    
    name = concat_text_from_tags(node, ['name'])
    if name in ['__attribute__']:
        if debug: print '  ignoring doxygen mis-parsing:', concat_text_from_tags(node, ['argsstring'])
        return

    if name.startswith('operator'): name = 'operator'
    if not context_name == None: symbol = context_name + '::' + name
    else: symbol = name

    publish = True

    is_function = kind == 'function'
    if publish: publish = kind != 'define'
    if publish and is_class: publish = is_function or static == 'yes'
    if publish and prot == 'private':
        if is_function: publish = node.attributes['virt'].value == 'virtual'
        else: publish =  False

    if publish and has_element(node, ['argsstring']): 
        publish = not get_text_for_element(node, 'argsstring').endswith('=0')
    
    if is_function: print_debug_info(node, ['kind', 'prot', 'static', 'virt'])
    else: print_debug_info(node, ['kind', 'prot', 'static'])
    if debug: print '  is_class:', is_class
    report(publish, symbol + '*')
    if is_function and node.attributes['virt'].value == 'virtual': report(publish,
                                                                          'non-virtual?thunk?to?' + symbol + '*')

def parse_compound_defs(xmldoc):
    compounddefs = xmldoc.getElementsByTagName('compounddef') 
    for node in compounddefs:
        kind = node.attributes['kind'].value

        if kind in ['page', 'file', 'example', 'union']: continue

        if kind in ['group']: 
            for member in node.getElementsByTagName('memberdef') : 
                parse_member_def(None, member, False)
            continue

        if kind in ['namespace']: 
            symbol = concat_text_from_tags(node, ['compoundname'])
            for member in node.getElementsByTagName('memberdef') : 
                parse_member_def(symbol, member, False)
            continue
        
        file = get_file_location(node)
        if debug: print '  from file:', file 
        if '/examples/' in file or '/test/' in file or '[generated]' in file or '[STL]' in file:
            continue

        if has_element(node, ['templateparamlist']): continue

        symbol = concat_text_from_tags(node, ['compoundname'])
        
        publish = True

        if publish: 
            if kind in ['class', 'struct']:
                prot =  node.attributes['prot'].value
                publish = prot != 'private'
                print_debug_info(node, ['kind', 'prot'])
                report(publish, 'vtable?for?' + symbol)
                report(publish, 'typeinfo?for?' + symbol)

        if publish: 
            for member in node.getElementsByTagName('memberdef') : 
                parse_member_def(symbol, member, kind in ['class', 'struct'])

if __name__ == "__main__":
    if len(argv) == 1 or '-h' in argv or '--help' in argv:
        print helptext
        exit()

    for arg in argv[1:]:
        try:
            if debug: print 'Processing:', arg
            xmldoc = minidom.parse(arg)
            parse_compound_defs(xmldoc)
        except Exception as error:
            print 'Error:', arg, error

    if debug: print 'Processing complete'

    print_report()