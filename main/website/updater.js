function reqListener() {
	const jsonObj = JSON.parse(this.responseText);

	document.getElementById("power-value").innerHTML = jsonObj.power.total;
	document.getElementById("power-l1-value").innerHTML = jsonObj.power.l1;
	document.getElementById("power-l2-value").innerHTML = jsonObj.power.l2;
	document.getElementById("power-l3-value").innerHTML = jsonObj.power.l3;
	document.getElementById("meter-import").innerHTML = jsonObj.meter.import.total;
  document.getElementById("meter-import-t1").innerHTML = jsonObj.meter.import.t1;
  document.getElementById("meter-import-t2").innerHTML = jsonObj.meter.import.t2;
	document.getElementById("meter-export").innerHTML = jsonObj.meter.export.total;
  document.getElementById("meter-export-t1").innerHTML = jsonObj.meter.export.t1;
  document.getElementById("meter-export-t2").innerHTML = jsonObj.meter.export.t2;
  document.getElementById("uptime-value").innerHTML = jsonObj.up;
  document.getElementById("uptime-value").style.color = "green";

	var currentPower = parseInt(jsonObj.power.total);
	if (currentPower < 0) {
		document.getElementById("power-text").style.color = "green";
		document.getElementById("power-status").innerHTML = "export";
	} else if (currentPower > 0) {
		document.getElementById("power-text").style.color = "red";
		document.getElementById("power-status").innerHTML = "import";
	} else {
		document.getElementById("power-text").style.color = "black";
		document.getElementById("power-status").innerHTML = "idle";
	}
}

function errorListener() {
  document.getElementById("uptime-value").innerHTML = "Not reachable";
  document.getElementById("uptime-value").style.color = "red";
}

function updateAll() {
	const req = new XMLHttpRequest();
	req.addEventListener("load", reqListener);
  req.addEventListener("error", errorListener);
	req.open("GET", "api/all");
	req.send();
}

updateAll();
setInterval(updateAll, 5000);