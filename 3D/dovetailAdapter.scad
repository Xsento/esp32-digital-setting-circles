// Parametry trapezu i wysokości
dolna_podstawa = 30;
gorna_podstawa = 22;
wysokosc_trapezu = 9;
dlugosc = 39; // Jak "gruby" ma być element

prostokatX = 12;
prostokatY = 4;
prostokatZ = 3.5;

stickX = 10;
stickY = 15;
holeDepth = 5;

difference(){
	// Extrude wyciąga płaski kształt wzdłuż osi Z
	linear_extrude(height = dlugosc) {
		polygon(points = [
		[0, 0],
		[dolna_podstawa, 0],     
		[(dolna_podstawa + gorna_podstawa)/2, wysokosc_trapezu],
		[(dolna_podstawa - gorna_podstawa)/2, wysokosc_trapezu]
		]);
	}
	
	translate([dolna_podstawa/2, wysokosc_trapezu-holeDepth/2, dlugosc/2])
	cube([stickX,holeDepth+0.1,stickY], center=true);
}


translate([(dolna_podstawa/2)-prostokatX/2,-prostokatY,0])
cube([prostokatX,prostokatY,prostokatZ]);