/*
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2020, Drew Kersnar <drewkersnar2021@u.northwestern.edu>
 * Copyright (c) 2020, Gaurav Chaudhary <gauravchaudhary2021@u.northwestern.edu>
 * Copyright (c) 2020, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2020, Brian Suchy <briansuchy2022@u.northwestern.edu>
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Drew Kersnar, Gaurav Chaudhary, Souradip Ghosh, 
 *          Brian Suchy, Peter Dinda 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include "../include/Protections.hpp"

/*
 * ---------- Constructors ----------
 */ 
ProtectionsHandler::ProtectionsHandler(
    Module *M,
    Noelle *N
) : M(M), N(N) 
{
    /*
     * Perform initial processing
     */ 
    _buildNonCanonicalAddress();
}


/*
 * ---------- Drivers ----------
 */ 
void ProtectionsHandler::Protect(void)
{
    /*
     * Iterate over all functions in @this->M, compute the DFA 
     * and calls to the runtime protections method if possible
     */ 
    for (auto &F : *M)
    {
        /*
         * Skip functions that are not instrumentable
         */ 
        if (!IsInstrumentable(F)) continue;


        /*
         * Compute the DFA for the current function 
         */ 
        ProtectionsDFA *PD = new ProtectionsDFA(
            &F, 
            N /* Noelle */
        );

        PD->Compute();


        /*
         * Inject guards
         */
        ProtectionsInjector *PI = new ProtectionsInjector(
            &F,
            TheDFA->FetchResult(),
            NonCanonical,
            N /* Noelle */
        );

        PI->Inject();
    }


    return;
}