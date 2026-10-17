internalCylinderHoleDiameter = 3;
internalCylinderRadius = 2.5;
internalCylinderHeight = 5;
legRadius = 3.5; 
plateX = 75;
plateY = 85;
plateZ = 6; //placeholder
spacing = 12 + 1; // battery shit thickness + safety

// actual distance between the internal cylinders is x = 63 and y = 73

perfboardX = 50;
perfboardY = 70;
perfboardSpacing = 4.5;
PBscrewXoffset = 3;
PBscrewYoffset = 2.5;
PBholeDiameter = 3.5;
PBsupportCylinderRadius = 5.5;
PBscrewHoleDepth = 9;
PBsupportOffsetX = 1;
PBsupportOffsetY = (PBscrewYoffset*PBsupportOffsetX)/PBscrewXoffset;

wireSlitX = perfboardX - PBsupportCylinderRadius*2 - 1.9;
wireSlitY = 5;

$fn = 50;
difference(){
    union(){
        difference(){
            union(){
                cube([plateX,plateY,plateZ]);
            
                translate([legRadius,  legRadius, -spacing-internalCylinderHeight]) 
                cylinder(h = spacing+internalCylinderHeight, r = legRadius);

                translate([plateX - legRadius, legRadius, -spacing-internalCylinderHeight]) 
                cylinder(h = spacing+internalCylinderHeight, r = legRadius);

                translate([legRadius, plateY -legRadius, -spacing-internalCylinderHeight]) 
                cylinder(h = spacing+internalCylinderHeight, r = legRadius);

                translate([plateX - legRadius, plateY -legRadius, -spacing-internalCylinderHeight]) 
                cylinder(h = spacing+internalCylinderHeight, r = legRadius);
                
            }
            // screw holes
            translate([legRadius, legRadius, -spacing-1]) 
            cylinder(h = spacing + plateZ+2, d = internalCylinderHoleDiameter);

            translate([plateX - legRadius, legRadius, -spacing-1]) 
            cylinder(h = spacing + plateZ+2, d = internalCylinderHoleDiameter);

            translate([legRadius, plateY -legRadius, -spacing-1]) 
            cylinder(h = spacing + plateZ+2, d = internalCylinderHoleDiameter);

            translate([plateX - legRadius, plateY -legRadius, -spacing-1]) 
            cylinder(h = spacing + plateZ+2, d = internalCylinderHoleDiameter);
            
            // mount holes
            translate([legRadius, legRadius, -spacing-internalCylinderHeight-1]) 
            cylinder(h = internalCylinderHeight+1, r = internalCylinderRadius);

            translate([plateX - legRadius, legRadius, -spacing-internalCylinderHeight-1]) 
            cylinder(h = internalCylinderHeight+1, r = internalCylinderRadius);

            translate([legRadius, plateY -legRadius, -spacing-internalCylinderHeight-1]) 
            cylinder(h = internalCylinderHeight+1, r = internalCylinderRadius);

            translate([plateX - legRadius, plateY -legRadius, -spacing-internalCylinderHeight-1]) 
            cylinder(h = internalCylinderHeight+1, r = internalCylinderRadius);    

            // perfboard hole
            translate([(plateX-perfboardX)/2, (plateY-perfboardY)/2, plateZ-perfboardSpacing])
            cube([perfboardX,perfboardY,perfboardSpacing+1]);
        
            // wire slit
            translate([(plateX-wireSlitX)/2, plateY - (plateY-perfboardY)/2-1, plateZ-perfboardSpacing])
            cube([wireSlitX,wireSlitY+1,perfboardSpacing+1]);
            
            translate([(plateX-wireSlitX)/2, (plateY-perfboardY)/2-wireSlitY, plateZ-perfboardSpacing])
            cube([wireSlitX,wireSlitY+1,perfboardSpacing+1]);
        }
        
        // perfboard supports
        translate([(plateX-perfboardX)/2 +PBsupportOffsetX, (plateY-perfboardY)/2 +PBsupportOffsetY, plateZ-perfboardSpacing]) 
        cylinder(h = perfboardSpacing, r = PBsupportCylinderRadius);
        
        translate([plateX-((plateX-perfboardX)/2+PBsupportOffsetX), (plateY-perfboardY)/2 +PBsupportOffsetY, plateZ-perfboardSpacing]) 
        cylinder(h = perfboardSpacing, r = PBsupportCylinderRadius);
        
        translate([(plateX-perfboardX)/2 +PBsupportOffsetX, plateY-((plateY-perfboardY)/2+PBsupportOffsetY), plateZ-perfboardSpacing]) 
        cylinder(h = perfboardSpacing, r = PBsupportCylinderRadius);
        
       translate([plateX-((plateX-perfboardX)/2+PBsupportOffsetX), plateY-((plateY-perfboardY)/2+PBsupportOffsetY), plateZ-perfboardSpacing]) 
        cylinder(h = perfboardSpacing, r = PBsupportCylinderRadius);
        
    }
    
    // perfboard screw holes
    translate([(plateX-perfboardX)/2 +PBscrewXoffset, (plateY-perfboardY)/2 +PBscrewYoffset, plateZ-PBscrewHoleDepth]) 
    cylinder(h = PBscrewHoleDepth+1, d = PBholeDiameter);
    
    translate([plateX-((plateX-perfboardX)/2+PBscrewXoffset), (plateY-perfboardY)/2 +PBscrewYoffset, plateZ-PBscrewHoleDepth]) 
    cylinder(h = PBscrewHoleDepth+1, d = PBholeDiameter);
    
    translate([(plateX-perfboardX)/2 +PBscrewXoffset, plateY-((plateY-perfboardY)/2+PBscrewYoffset), plateZ-PBscrewHoleDepth])
    cylinder(h = PBscrewHoleDepth+1, d = PBholeDiameter);
    
    translate([plateX-((plateX-perfboardX)/2+PBscrewXoffset), plateY-((plateY-perfboardY)/2+PBscrewYoffset), plateZ-PBscrewHoleDepth])
    cylinder(h = PBscrewHoleDepth+1, d = PBholeDiameter);
}

