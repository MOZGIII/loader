// Generated by CoffeeScript 1.3.3
(function() {

  this.Uploader = (function() {

    function Uploader(file) {
      this.file = file;
      this.chunk_size = 1024 * 1024 * 5;
      this.session_id = null;
      this.server_address = "localhost:8080";
      this.main_socket = null;
      this.total_chunks = Math.ceil(this.file.size / this.chunk_size);
    }

    Uploader.prototype.server_base_url = function() {
      return "ws://" + this.server_address + "/upload";
    };

    Uploader.prototype.upload = function() {
      return this.get_session(this.start_upload, function() {
        return this.log("Unable to get session");
      });
    };

    Uploader.prototype.get_session = function(on_success, on_failure) {
      this.session_id = null;
      this.main_socket = new WebSocket(this.server_base_url());
      this.main_socket.master = this;
      this.main_socket.onopen = function(event) {
        this.master.log('main: onopen');
        this.send("CONTROLLER");
        this.state = "init";
        return this.send(["INFO", this.master.file.name, this.master.file.size, this.master.chunk_size].join("\n"));
      };
      this.main_socket.onmessage = function(event) {
        this.master.log(("main [" + this.state + "]: onmessage, ") + event.data);
        if (this.state === "init") {
          if (event.data === "ok") {
            this.state = "waiting_for_session_id";
          }
          if (this.state !== "waiting_for_session_id") {
            return this.close();
          }
        } else {
          this.master.session_id = event.data;
          return this.close();
        }
      };
      return this.main_socket.onclose = function(event) {
        this.master.log('main: onclose');
        if (!this.master.session_id) {
          return on_failure.call(this.master, "Unable to get session id (" + event.data + ")");
        } else {
          return on_success.call(this.master);
        }
      };
    };

    Uploader.prototype.start_upload = function() {
      this.current_chunk_number = -1;
      return this.on_chunk_upload_complete();
    };

    Uploader.prototype.upload_done = function() {
      return alert("File uploaded succesfully!");
    };

    Uploader.prototype.on_chunk_upload_complete = function() {
      if (this.current_chunk_number === this.total_chunks - 1) {
        return this.upload_done();
      } else {
        this.current_chunk_number += 1;
        console.log("Uploading chunk #" + this.current_chunk_number + " [" + (this.current_chunk_number + 1) + "/" + this.total_chunks + "]");
        return this.upload_chunk(this.current_chunk_number, this.on_chunk_upload_complete, function(errmsg) {
          if (errmsg == null) {
            errmsg = "Unknown error";
          }
          return this.log("Unable to upload chunk #" + this.current_chunk_number + ": " + errmsg);
        });
      }
    };

    Uploader.prototype.upload_chunk = function(chunk_number, on_done, on_error) {
      var chunk, tansport_socket;
      if (!(chunk = this.get_data_chunk(chunk_number))) {
        return on_error.call("Unable to get chunk");
      } else {
        tansport_socket = new WebSocket(this.server_base_url());
        tansport_socket.master = this;
        tansport_socket.transfer_succeed = false;
        tansport_socket.onopen = function(event) {
          this.master.log("tsc #" + chunk_number + ": onopen");
          this.send("TRANSMITTER");
          this.state = "init";
          return this.send(this.master.session_id);
        };
        tansport_socket.onmessage = function(event) {
          this.master.log(("tsc #" + chunk_number + " [" + this.state + "]: onmessage, ") + event.data);
          if (this.state === "init") {
            if (event.data === "ok") {
              this.state = "sent_chunk_info";
            }
            if (this.state !== "sent_chunk_info") {
              this.close;
            }
            return this.send(chunk_number);
          } else if (this.state === "sent_chunk_info") {
            if (event.data === "ok") {
              this.state = "sending_chunk";
            }
            if (this.state !== "sending_chunk") {
              this.close;
            }
            return this.send(chunk);
          } else if (this.state === "sending_chunk") {
            this.transfer_succeed = event.data === "ok";
            this.state = "done";
            return this.close();
          }
        };
        return tansport_socket.onclose = function(event) {
          this.master.log("tsc #" + chunk_number + ": onclose");
          if (this.transfer_succeed) {
            return on_done.call(this.master);
          } else {
            return on_error.call(this.master, "Upload was not successfull");
          }
        };
      }
    };

    Uploader.prototype.get_data_chunk = function(chunk_number) {
      if (chunk_number * this.chunk_size > this.file.size) {
        return null;
      } else {
        if (this.file.slice != null) {
          return this.file.slice(chunk_number * this.chunk_size, (chunk_number + 1) * this.chunk_size, "application/octet-stream");
        } else if (this.file.webkitSlice != null) {
          return this.file.webkitSlice(chunk_number * this.chunk_size, (chunk_number + 1) * this.chunk_size, "application/octet-stream");
        } else if (this.file.mozSlice != null) {
          return this.file.mozSlice(chunk_number * this.chunk_size, (chunk_number + 1) * this.chunk_size, "application/octet-stream");
        } else {
          return alert("File.slice in unsupported in this browser!");
        }
      }
    };

    Uploader.prototype.log = function(text) {
      return console.log(text);
    };

    return Uploader;

  })();

}).call(this);