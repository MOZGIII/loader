function updateProgress(evt) {
	console.log("Transferring.");
	if (evt.lengthComputable) {
		var percentComplete = evt.loaded / evt.total;
		console.log(evt.percentComplete);
	}
}

function transferComplete(evt) {
	alert("Transfer complete.");
}

function sendFile(/* String */ base64fileData,/* String */ fileName) {
	var boundary = "xxxxxxxxx";
	var uri = "index.php?upload";

	if (XMLHttpRequest) xhr = new XMLHttpRequest();
	else return false;

	xhr.addEventListener("progress", updateProgress, false);
	xhr.addEventListener("load", transferComplete, false);
	//xhr.addEventListener("error", transferFailed, false);
	//xhr.addEventListener("abort", transferCanceled, false);

	xhr.open("POST",uri,true);
	// Simulate a file MIME POST request.
	xhr.setRequestHeader("Content-Type", "multipart/form-data,boundary="+boundary);

	xhr.onreadystatechange = function()
		{
		if (xhr.readyState==4)
			if ((xhr.status >= 200 && xhr.status <= 200) || xhr.status == 304)
				{
				// Handle response.
				if (xhr.responseText!="")
					alert(xhr.responseText); // handle response.
				}
		}

	var body = "–" + boundary + "rn";
	body += "Content-Disposition: form-data; name='myFile'; filename='" + fileName + "'rn";
	body += "Content-Type: application/octet-streamrnrn";
	body += base64fileData + "rn";
	body += "–" + boundary + "–";

	xhr.sendAsBinary(body);
}

function manageFile(file) {
	var fileReader = new FileReader();
	console.log("Start messing with: " + file.name);
	
	fileReader.onload = function(read) {
		// readAsBinaryString():
		// async methods with callback -> result here.
		
		sendFile(read.target.result, file.name);
	}

	fileReader.readAsBinaryString(file);
}


window.onload = function() {
	// Chrome compatibility (http://royaltutorials.com/object-has-no-method-sendasbinary/).
	if(!XMLHttpRequest.prototype.sendAsBinary){
		XMLHttpRequest.prototype.sendAsBinary = function(datastr) {
			function byteValue(x) { return x.charCodeAt(0) & 0xff; }
			var ords = Array.prototype.map.call(datastr, byteValue);
			var ui8a = new Uint8Array(ords);
			this.send(ui8a.buffer);
		}
	}

	document.getElementById("dropzone").ondragenter = function(event) {
		event.stopPropagation();
		event.preventDefault();
	}

	document.getElementById("dropzone").ondragover = function(event) {
		event.stopPropagation();
		event.preventDefault();
	}

	document.getElementById("dropzone").ondrop = function(event) {
		event.stopPropagation();
		event.preventDefault();

		var files = event.dataTransfer.files;
		for (var i = 0; i < files.length; i++) {
			// console.log(i);
			manageFile(files.item(i));
		}
	}
}