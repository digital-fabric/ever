# Ever - a callback-less event reactor for Ruby

[![Gem Version](https://badge.fury.io/rb/ever.svg)](http://rubygems.org/gems/ever)
[![Ever Test](https://github.com/digital-fabric/ever/workflows/Tests/badge.svg)](https://github.com/digital-fabric/ever/actions?query=workflow%3ATests)
[![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/digital-fabric/ever/blob/master/LICENSE)

Ever is a [libev](http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod)-based event reactor for Ruby with a callback-less design. Events are emitted to an application-provided block inside a tight loop without registering and invoking callbacks.

## Features

- Simple, minimalistic API
- Zero dependencies
- Callback-less API for getting events
- Events for I/O readiness
- Events for one-shot or recurring timers
- Cross-thread signalling and emitting of events

## Rationale

I'm planning to add a compatibility mode to [Tipi](https://github.com/digital-fabric/tipi), a new [Polyphony](https://github.com/digital-fabric/polyphony)-based web server for Ruby. In this mode, Tipi will not be using Polyphony, but will employ multiple worker threads for handling concurrent requests.

After coming up with lots of ideas for how to achieve this, I settled on the following design:

- The main thread runs a libev-based event reactor, and deals with accepting connections and distributing events.
- One or more worker threads wait for jobs to execute.
- When a new connection is accepted, the main thread starts watching for I/O readiness.
- When a connection is ready for reading, the main threads puts the connection on the job queue.
- A worker thread shifts the connection from the job queue and tries to read an incoming request. If the request is not complete, the connection is watched again for read readiness.
- When the request is complete, the worker threads continues to run the Rack app, gets the response, and tries to write the response. If the response cannot be written, the connection is watched for write readiness.
- When the response has been written, the connection is watched again for read readiness in preparation for the next request.

What's interesting about this design is that any number of worker threads can (theoretically) handle any number of concurrent requests, since each worker thread is not tied to a specific connection, but rather work on each connection in the queue as it becomes ready (for reading or writing).

## Installing

If you're using bundler just add it to your `Gemfile`:

```ruby
source 'https://rubygems.org'

gem 'ever'
```

You can then run `bundle install` to install it. Otherwise, just run `gem install ever`.

## Usage

Start by creating an instance of Ever::Loop:

```ruby
require 'ever'

evloop = Ever::Loop.new
```

### Setting up event watchers

All events are identified using an application-provided key. This means that your app should provide a unique key for each event you wish to watch. To watch for I/O readiness, use `Loop#watch_io(key, io, read_write, oneshot)` where:

- `key`: unique event key (this can be *any* value, and in many cases you can just use the `IO` instance.)
- `io`: `IO` instance to watch.
- `read_write`: `false` for read, `true` for write.
- `oneshot`: `true` for one-shot event monitoring, `false` otherwise.

Example:

```ruby
result = socket.read_nonblock(16384, exception: false)
case result
when :wait_readable
  evloop.watch_io(socket, socket, false, true)
else
  ...
end
```

To setup up timers, use `Loop#watch_timer(key, duration, interval)` where:

- `key`: unique event key
- `duration`: timer duration in seconds.
- `interval`: recurring interval in seconds. `0` for a one-shot timer.

```ruby
evloop.watch_timer(:timer, 1, 1)
evloop.each do |key|
  case key
  when :timer
    puts "Got timer event"
  end
end
```

### Stopping watchers

To stop a specific watcher, use `Loop#unwatch(key)` and provide the key previously provided to `#watch_io` or `#watch_timer`:

```ruby
evloop.watch_timer(:timer, 1, 1)
count = 0
evloop.each do |key|
  case key
  when :timer
    puts "Got timer event"
    count += 1
    evloop.unwatch(:timer) if count == 10
  end
end
```

### Processing events

To process events as they happen, use `Loop#each`, which will block waiting for events and will yield events as they happen. The application-provided block will be called with the event key for each event:

```ruby
evloop.each do |key|
  distribute_event(key)
end
```

Alternatively you can use `Loop#next_event` to process events one by one, or using a custom loop. Note that while this method can block, it can also return `nil` in case no event was generated.

### Emitting custom events

You can emit events using `Loop#emit(key)`. In case the event loop is currently polling for events, it immediately return and the emitted event will be available.

### Signalling the event loop

You can signal the event loop in order to stop it from blocking by using `Loop#signal`.

### Stopping the event loop

An event loop that is currently blocking on `Loop#each` can be stopped using `Loop#stop` or by calling `Loop#emit(:stop)`.

### Signal handling

The created event loop will not trap signals by itself. You can setup signal traps and emit events that tell the app what to do. Here's an example:

```ruby
evloop = Ever::Loop.new
trap('SIGINT') { evloop.stop }
evloop.each { |key| handle_event(key) }
```

## API Summary

|Method|Description|
|------|-----------|
|`Loop.new()`|create a new event loop.|
|`Loop#each { |key| ... }`|Handle events in an infinite loop.|
|`Loop#next_event`|Wait for an event and return its key.|
|`Loop#watch_io(key, io, read_write, oneshot)`|Watch an IO instance for readiness.|
|`Loop#watch_timer(key, duration, interval)`|Setup a one-shot/recurring timer.|
|`Loop#unwatch(key)`|Stop watching specific event key.|
|`Loop#emit(key)`|Emit a custom event.|
|`Loop#signal`|Signal the event loop, causing it to break if currently blocking.|
|`Loop#stop`|Stop an event loop currently blocking in `#each`.|

## Performance

I did not yet explore all the performance implications of this new design, but [a sketch I made for an HTTP server]() shows it performing consistently at >60000 reqs/seconds on my development machine.

## Contributing

Issues and pull requests will be gladly accepted. If you have found this gem
useful, please let me know.