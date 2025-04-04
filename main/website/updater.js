function reqListener() {
	const jsonObj = JSON.parse(this.responseText);

	document.getElementById("power-value").innerHTML = jsonObj.power.total;
	document.getElementById("power-l1-value").innerHTML = jsonObj.power.l1;
	document.getElementById("power-l2-value").innerHTML = jsonObj.power.l2;
	document.getElementById("power-l3-value").innerHTML = jsonObj.power.l3;
	document.getElementById("meter-import").innerHTML = jsonObj.meter.import;
	document.getElementById("meter-export").innerHTML = jsonObj.meter.export;

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

function updateAll() {
	const req = new XMLHttpRequest();
	req.addEventListener("load", reqListener);
	req.open("GET", "/api/all");
	req.send();
}

updateAll();
setInterval(updateAll, 5000);