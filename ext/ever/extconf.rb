# frozen_string_literal: true

require 'rubygems'
require 'mkmf'

$defs << '-DEV_USE_LINUXAIO'     if have_header('linux/aio_abi.h')
$defs << '-DEV_USE_SELECT'       if have_header('sys/select.h')
$defs << '-DEV_USE_POLL'         if have_type('port_event_t', 'poll.h')
$defs << '-DEV_USE_EPOLL'        if have_header('sys/epoll.h')
$defs << '-DEV_USE_KQUEUE'       if have_header('sys/event.h') && have_header('sys/queue.h')
$defs << '-DEV_USE_PORT'         if have_type('port_event_t', 'port.h')
$defs << '-DHAVE_SYS_RESOURCE_H' if have_header('sys/resource.h')  

$CFLAGS << " -Wno-comment"
$CFLAGS << " -Wno-unused-result"
$CFLAGS << " -Wno-dangling-else"
$CFLAGS << " -Wno-parentheses"

CONFIG['optflags'] << ' -fno-strict-aliasing' unless RUBY_PLATFORM =~ /mswin/

dir_config 'ever_ext'
create_makefile 'ever_ext'
