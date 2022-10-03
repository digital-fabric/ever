# frozen_string_literal: true

require 'bundler/setup'
require 'nio'
require 'http/parser'
require 'socket'

class Connection
  attr_reader :io, :parser, :request_complete,
              :request_headers, :request_body
  attr_accessor :response, :monitor

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

  def emit_monitor(event)
    $directive_queue << [:monitor, self, event]
    $o << '.'
  end

  def emit_close
    $directive_queue << [:close, self]
    $o << '.'
  end

  def monitor(event)
    @monitor&.close
    @monitor = $selector.register(@io, event)
    @monitor.value = self
  rescue IOError
    @monitor = nil
  end

  def close
    @monitor&.close
    @io.close
  end
end

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
    conn.emit_monitor(:r)
  when :wait_writable
    conn.emit_monitor(:w)
  when nil
    conn.emit_close
  else
    conn.parser << result
    if conn.request_complete
      conn.response = handle_request(conn.request_headers, conn.request_body)
      handle_write_response(conn)
    else
      conn.emit_monitor(:r)
    end
  end
rescue HTTP::Parser::Error, SystemCallError, IOError
  conn.emit_close
end

def handle_request(headers, body)
  response_body = "Hello, world!"
  "HTTP/1.1 200 OK\nContent-Length: #{response_body.bytesize}\n\n#{response_body}"
end

def handle_write_response(conn)
  result = conn.io.write_nonblock(conn.response, exception: false)
  case result
  when :wait_readable
    conn.emit_monitor(:r)
  when :wait_writable
    conn.emit_monitor(:w)
  when nil
    conn.emit_close
  else
    conn.setup_read_request
    conn.emit_monitor(:r)
  end
end

def setup_connection(io)
  # happens in the main thread
  conn = Connection.new(io)
  conn.monitor(:r)
end

num_workers = ARGV[0] ? ARGV[0].to_i : 1
if num_workers < 1
  puts "Invalid number of worker threads: #{ARGV[0].inspect}"
  exit!
end

server = TCPServer.new('0.0.0.0', 1234)
puts "Listening on port 1234..."
trap('SIGINT') { exit! }

$selector = NIO::Selector.new
$i, $o = IO.pipe
$selector.register(server, :r)
$selector.register($i, :r)

$job_queue = Queue.new
$directive_queue = Queue.new

puts "Starting #{num_workers} worker threads..."
num_workers.times do
  Thread.new do
    while (job = $job_queue.shift)
      handle_connection(job)
    end
  end
end

loop do
  $selector.select do |monitor|
    case monitor.io
    when server
      socket = server.accept
      setup_connection(socket)
    when $i
      $i.read(1) # flush pipe
      while !$directive_queue.empty?
        k, c, e = $directive_queue.shift
        case k
        when :monitor
          c.monitor(e)
        when :close
          c.close
        end
      end
    else
      $job_queue << monitor.value
    end
  end
end
