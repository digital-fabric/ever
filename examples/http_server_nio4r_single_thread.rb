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

  def monitor(event)
    @monitor&.close
    @monitor = $selector.register(@io, event)
    @monitor.value = self
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
    conn.monitor(:r)
  when :wait_writable
    conn.monitor(:w)
  when nil
    conn.close
  else
    conn.parser << result
    if conn.request_complete
      conn.response = handle_request(conn.request_headers, conn.request_body)
      handle_write_response(conn)
    else
      conn.monitor(:r)
    end
  end
rescue HTTP::Parser::Error, SystemCallError, IOError
  conn.close
end

def handle_request(headers, body)
  response_body = "Hello, world!"
  "HTTP/1.1 200 OK\nContent-Length: #{response_body.bytesize}\n\n#{response_body}"
end

def handle_write_response(conn)
  result = conn.io.write_nonblock(conn.response, exception: false)
  case result
  when :wait_readable
    conn.monitor(:r)
  when :wait_writable
    conn.monitor(:w)
  when nil
    conn.close
  else
    conn.setup_read_request
    conn.monitor(:r)
  end
end

def setup_connection(io)
  conn = Connection.new(io)
  conn.monitor(:r)
end

server = TCPServer.new('0.0.0.0', 1234)
puts "Listening on port 1234..."
trap('SIGINT') { exit! }

$selector = NIO::Selector.new

$selector.register(server, :r)

loop do
  $selector.select do |monitor|
    case monitor.io
    when server
      socket = server.accept
      setup_connection(socket)
    else
      handle_connection(monitor.value)
    end
  end
end
