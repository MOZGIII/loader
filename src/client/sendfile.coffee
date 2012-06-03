class @Uploader
	constructor: (@file) -> 
		@chunk_size = 1024*1024*5 # 5 MB
		@session_id = null
		@server_address = "localhost:8080"
		@main_socket = null
		
		@total_chunks = Math.ceil(@file.size / @chunk_size)
		
	server_base_url: ->
		"ws://#{@server_address}/upload"
	
	upload: ->
		@get_session @start_upload, ->
			@log "Unable to get session"
		
	get_session: (on_success, on_failure) ->
		@session_id = null
		@main_socket = new WebSocket @server_base_url()
		@main_socket.master = this
		@main_socket.onopen = (event) ->
			@master.log('main: onopen');
			@send("CONTROLLER")
			@state = "init"
			@send([
					"INFO",
					@master.file.name,
					@master.file.size,
					@master.chunk_size
				].join("\n"));

		@main_socket.onmessage = (event) ->
			@master.log("main [#{@state}]: onmessage, " + event.data);
			if @state == "init"
				@state = "waiting_for_session_id" if event.data == "ok"
				@close() unless @state == "waiting_for_session_id"
			else
				@master.session_id = event.data
				@close();

		@main_socket.onclose = (event) ->
			@master.log('main: onclose');
			unless @master.session_id
				on_failure.call @master, "Unable to get session id (#{event.data})"
			else
				on_success.call @master
		
	start_upload: ->
		# Do some magic here
		@current_chunk_number = -1
		@on_chunk_upload_complete()
		
	upload_done: ->
		alert "File uploaded succesfully!"
	
	on_chunk_upload_complete: ->
		if @current_chunk_number == @total_chunks - 1
			@upload_done()
		else
			@current_chunk_number += 1
			console.log("Uploading chunk ##{@current_chunk_number} [#{@current_chunk_number + 1}/#{@total_chunks}]");
			@upload_chunk @current_chunk_number, @on_chunk_upload_complete, (errmsg = "Unknown error") ->
				@log "Unable to upload chunk ##{@current_chunk_number}: #{errmsg}"
	
	upload_chunk: (chunk_number, on_done, on_error) ->
		unless chunk = @get_data_chunk chunk_number
			on_error.call "Unable to get chunk"
		else
			tansport_socket = new WebSocket @server_base_url()
			tansport_socket.master = this
			tansport_socket.transfer_succeed = false
			tansport_socket.onopen = (event) ->
				@master.log("tsc ##{chunk_number}: onopen");
				@send("TRANSMITTER")
				@state = "init"
				@send(@master.session_id);

			tansport_socket.onmessage = (event) ->
				@master.log("tsc ##{chunk_number} [#{@state}]: onmessage, " + event.data);
				if @state == "init"
					@state = "sent_chunk_info" if event.data == "ok"
					@close unless @state == "sent_chunk_info"
					@send(chunk_number);
				else if @state == "sent_chunk_info"
					@state = "sending_chunk" if event.data == "ok"
					@close unless @state == "sending_chunk"
					@send(chunk);
				else if @state == "sending_chunk"
					@transfer_succeed = event.data == "ok"
					@state = "done"
					@close();

			tansport_socket.onclose = (event) ->
				@master.log("tsc ##{chunk_number}: onclose");
				if @transfer_succeed
					on_done.call @master
				else
					on_error.call @master, "Upload was not successfull"
	
	# private
	
	get_data_chunk: (chunk_number) ->
		if chunk_number * @chunk_size > @file.size
			null
		else
			if @file.slice?
				@file.slice chunk_number * @chunk_size, (chunk_number + 1) * @chunk_size, "application/octet-stream"
			else if @file.webkitSlice?
				@file.webkitSlice chunk_number * @chunk_size, (chunk_number + 1) * @chunk_size, "application/octet-stream"
			else if @file.mozSlice?
				@file.mozSlice chunk_number * @chunk_size, (chunk_number + 1) * @chunk_size, "application/octet-stream"
			else
				alert "File.slice in unsupported in this browser!"
		
	log: (text) ->
		console.log text

