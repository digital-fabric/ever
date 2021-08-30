# frozen_string_literal: true

require_relative 'helper'
require 'ever'

class LoopTest < MiniTest::Test
  def setup
    super
    @loop = Ever::Loop.new
  end

  def test_no_watchers
    @loop.emit(1)
    @loop.emit(2)
    @loop.emit(3)
    buf = []
    @loop.each do |key|
      buf << key
      @loop.emit(:stop) if key == 3
    end
    assert_equal [1, 2, 3], buf
  end

  def test_io
    i, o = IO.pipe
    
    @loop.watch_io('foo', i, false, true)

    o << 'foo'
    buf = []
    @loop.each do |key|
      buf << key
      @loop.stop
    end

    assert_equal ['foo'], buf
  end

  def test_cross_thread_signalling
    i, o = IO.pipe
    @loop.watch_io('foo', i, false, true)

    Thread.new { @loop.signal }

    event = @loop.next_event
    assert_nil nil, event
  end

  def test_emit
    i, o = IO.pipe
    @loop.watch_io('foo', i, false, true)

    Thread.new { o << 'bar'; @loop.emit('baz') }

    event = @loop.next_event
    assert_equal 'baz', event

    event = @loop.next_event
    assert_equal 'foo', event
  end

  def test_timer_oneshot
    @loop.watch_timer('foo', 0.01, 0)

    t0 = Time.now
    (event = @loop.next_event) until event;
    t1 = Time.now

    assert_equal 'foo', event
    assert_in_range 0.005..0.02, t1 - t0
  end

  def test_timer_recurring
    @loop.watch_timer('foo', 0.01, 0.01)

    t0 = Time.now
    buf = []
    @loop.each do |key|
      buf << Time.now if key == 'foo'
      @loop.stop if buf.size == 3
    end
    t1 = Time.now

    assert_equal 3, buf.size
    assert_in_range 0.005..0.04, t1 - t0
  end
end
