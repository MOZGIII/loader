require 'socket'

module SessionsStore
  def self.mutex
    @mutex ||= Mutex.new
  end

  def self.counter
    @counter ||= 0
  end
  
  def self.counter_inc
    @counter ||= 0
    @counter += 1
  end
  
  def self.store
    @store ||= {}
  end
  
  def self.add value
    store[value.session_id] = value
  end
  
  class Session
  
    attr_reader :session_id
    attr_accessor :file_name, :file_size, :chunk_size 
    def initialize(options = {})
      options.each { |key, value| send("#{key}=".to_sym, value) }
      SessionsStore.mutex.synchronize do
        @session_id = SessionsStore.counter_inc
        SessionsStore.add self
      end
    end
      
  end

end

def send_message socket, lines
  lines.each do |line|
    f = socket.send "#{line.to_s}\r\n".encode("ASCII-8BIT"), 0
    return f if f <= 0
  end
  socket.send "\r\n".encode("ASCII-8BIT"), 0
end

def read_message socket
  str = ""
  while str[-4, 4] != "\r\n\r\n"
    data = socket.recv(1)
    return false if data.size == 0
    str << data
  end
  str
end

server = TCPServer.new("127.0.0.1", 3713)
loop do
  Thread.start(server.accept) do |client|
  #client = server.accept
    puts "New connection to a database server"
    
    while request = read_message(client)
      lines = request.chomp.split
      case lines[0].downcase.to_sym
      when :upload
        session = SessionsStore::Session.new({
          file_name: lines[1].chomp,
          file_size: lines[2].to_i,
          chunk_size: lines[3].to_i
        })
        puts "New upload request for file: #{session.file_name}"
        send_message client, ["ACCEPTED", session.session_id]
      when :upinfo
        session_id = lines[1].to_i
        session = SessionsStore.store[session_id]
        puts "Upload info request for session #{session_id} (#{session.file_name})"
        send_message client, ["OK", session.chunk_size, "uploads/#{session_id}_#{session.file_name}.tmp"]
      else
        puts "Invalid packet! #{lines[0]}"
      end       
    end
    
    puts "Closing the connection"
    client.close
  end
end