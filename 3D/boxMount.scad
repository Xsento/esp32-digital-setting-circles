plateSurplusX = 4;
plateSurplusY = 5;

screwX = 59;
screwY = 102;
screwHoleRadius = 2;

plateX = plateSurplusX*2+screwX;
plateY = plateSurplusY*2+screwY;
plateZ = 4;

stickX = 10;
stickY = 15;
stickZ = 20;

$fn = 20;

difference(){
    cube([plateX,plateY,plateZ]);

    translate([plateSurplusX, plateSurplusY, -1])
    cylinder(h = plateZ+2, r = screwHoleRadius);
    
    translate([plateSurplusX+screwX, plateSurplusY, -1])
    cylinder(h = plateZ+2, r = screwHoleRadius);
	
    translate([plateSurplusX, plateSurplusY+screwY, -1])
    cylinder(h = plateZ+2, r = screwHoleRadius);
    
    translate([plateSurplusX+screwX, plateSurplusY+screwY, -1])
	cylinder(h = plateZ+2, r = screwHoleRadius);
}

translate([plateX/2, plateY/2, plateZ+stickZ/2])
cube([stickX,stickY,stickZ], center = true);

translate([plateX/2, plateY/2, plateZ])
ostroslup_prostokatny(stickX*2,stickY*2,20);

module ostroslup_prostokatny(szerokosc_x, dlugosc_y, wysokosc) {
    x = szerokosc_x / 2;
    y = dlugosc_y / 2;
    
    polyhedron(
        points = [
            [-x, -y, 0],  // 0: lewy dolny narożnik
            [ x, -y, 0],  // 1: prawy dolny narożnik
            [ x,  y, 0],  // 2: prawy górny narożnik
            [-x,  y, 0],  // 3: lewy górny narożnik
            [ 0,  0, wysokosc] // 4: czubek ostrosłupa
        ],
        faces = [
            [0, 1, 2, 3], // Podstawa (patrząc od dołu, zgodnie z ruchem wskazówek zegara)
            [1, 0, 4],    // Przód
            [2, 1, 4],    // Prawo
            [3, 2, 4],    // Tył
            [0, 3, 4]     // Lewo
        ]
    );
}