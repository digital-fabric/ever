# frozen_string_literal: true

require 'bundler/setup'
require 'ever'
require 'http/parser'
require 'socket'

class Connection
  attr_reader :io, :parser, :request_complete,
              :request_headers, :request_body
  attr_accessor :response

  def initialize(io)
    @io = io
    @parser = Http::Parser.new(self)
    setup_read_request
  end

  def setup_read_request
    @request_complete = nil
    @request_headers = nil
    @request_body = +''
  end

  def on_headers_complete(headers)
    @request_headers = headers
  end

  def on_body(chunk)
    @request_body << chunk
  end

  def on_message_complete
    @request_complete = true
  end
end

$job_queue = Queue.new
$evloop = Ever::Loop.new

def handle_connection(conn)
  if !conn.request_complete
    handle_read_request(conn)
  else
    handle_write_response(conn)
  end
end

def handle_read_request(conn)
  result = conn.io.read_nonblock(16384, exception: false)
  case result
  when :wait_readable
    $evloop.emit([:watch_io, conn, false, true])
  when :wait_writable
    $evloop.emit([:watch_io, conn, true, true])
  when nil
    $evloop.emit([:close, conn])
  else
    conn.parser << result
    if conn.request_complete
      conn.response = handle_request(conn.request_headers, conn.request_body)
      handle_write_response(conn)
    else
      $evloop.emit([:watch_io, conn, false, true])
    end
  end
rescue HTTP::Parser::Error, SystemCallError, IOError
  $evloop.emit([:close, conn])
end

def handle_request(headers, body)
  response_body = "Hello, world!"
  "HTTP/1.1 200 OK\nContent-Length: #{response_body.bytesize}\n\n#{response_body}"
end

def handle_write_response(conn)
  result = conn.io.write_nonblock(conn.response, exception: false)
  case result
  when :wait_readable
    $evloop.emit([:watch_io, conn, false, true])
  when :wait_writable
    $evloop.emit([:watch_io, conn, true, true])
  when nil
    $evloop.emit([:close, conn])
  else
    conn.setup_read_request
    $evloop.emit([:watch_io, conn, false, true])
  end
end

def setup_connection(io)
  conn = Connection.new(io)
  $evloop.emit([:watch_io, conn, false, true])
end

num_workers = ARGV[0] ? ARGV[0].to_i : 1
if num_workers < 1
  puts "Invalid number of worker threads: #{ARGV[0].inspect}"
  exit!
end

server = TCPServer.new('0.0.0.0', 1234)
puts "Listening on port 1234..."
trap('SIGINT') { $evloop.stop }
$evloop.watch_io(:accept, server, false, false)

puts "Starting #{num_workers} worker threads..."
num_workers.times do
  Thread.new do
    while (job = $job_queue.shift)
      handle_connection(job)
    end
  end
end

$evloop.each do |event|
  case event
  when :accept
    socket = server.accept
    setup_connection(socket)
  when Connection
    $job_queue << event
  when Array
    cmd = event[0]
    case cmd
    when :watch_io
      $evloop.watch_io(event[1], event[1].io, event[2], event[3])
    when :close
      conn = event[1]
      conn.io.close
    end
  end
end
