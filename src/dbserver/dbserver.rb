require 'socket'

module SessionsStore
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
  
  class Session
  
    attr_reader :session_id, :file_name, :file_size, :chunk_size 
    def initialize
      @session_id = SessionsStore.counter_inc
    end
  
  end

end


server = TCPServer.new("127.0.0.1", 3713)
loop do
  Thread.start(server.accept) do |client|
    puts "New connection to a database server"
    
    while request = client.read
      break if request.size == 0
      puts "Got something: "
      puts request
      
    end
    
    puts "Closing the connection"
    client.close
  end
end