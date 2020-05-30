/* MSE */

#include "evolve.h"
#include "binary_evolution.h"

extern "C"
{

//struct ktype__ ktype_;

int handle_binary_evolution(ParticlesMap *particlesMap, double t_old, double t, double *dt_binary_evolution, int *integration_flag)
{
    if (*integration_flag != 0) /* Only do binary evolution if we can be sure the system is dynamically stable and we have well-defined orbits. */
    {
        return 0;
    }
    update_structure(particlesMap);
    handle_wind_accretion(particlesMap,t_old,t,dt_binary_evolution,integration_flag);
    handle_mass_transfer(particlesMap,t_old,t,dt_binary_evolution,integration_flag);
        
    //*dt_binary_evolution = 1.0e4;
    
    bool stable = check_system_for_dynamical_stability(particlesMap, integration_flag);
    if (stable == false)
    {
        *integration_flag = 1;
    }

    return 0;
}



int handle_mass_transfer(ParticlesMap *particlesMap, double t_old, double t, double *dt_binary_evolution, int *integration_flag)
{
    *dt_binary_evolution = 1.0e100;
    double min_dt = 1.0e0;
    double dt = t - t_old; /* Imposed timestep which will be used for secular ODE integration. */

    double epsilon_MT = 0.01;
    double dt_temp;

    //int kw1=8;
    //int kw2=11;
    //int knew = ktype_(&kw1,&kw2) - 100;
    //int knew = ktype_(&kw1,&kw2) - 100;
    //int knew = determine_merger_type(kw1,kw2);
    //printf("knew %d\n",);

    ParticlesMapIterator it_p;
    for (it_p = particlesMap->begin(); it_p != particlesMap->end(); it_p++)
    {

        Particle *donor = (*it_p).second;
        //printf("handle_mass_transfer donor %d is_binary %d is_bound %d donor->RLOF_flag %d\n",donor->index,donor->is_binary,donor->is_bound,donor->RLOF_flag);
        if (donor->is_binary == false and donor->is_bound == true)
        {
            
            if (donor->RLOF_flag == 1)
            {
                int flag = handle_mass_transfer_cases(particlesMap, donor->parent, donor->index, donor->sibling, integration_flag, t_old, t, dt_binary_evolution);

                #ifdef IGNORE
                if (flag == 1)
                {
                    dt_temp = compute_orbital_period(parent);
                    common_envelope_evolution(particlesMap, parent->index, donor->index, accretor->index, integration_flag);
                }
                else /* TO DO: include low-mass dynamical mass transfer case (evolv2.f: lines 1150 - 1233 */
                {
                   dt_temp = compute_orbital_period(parent); /* TO DO: change this to something more appropriate */
                    compute_stable_mass_transfer_properties(particlesMap, parent->index, donor->index, accretor->index, integration_flag);
                }
                
                //dt_temp = epsilon_MT*fabs(donor->mass/donor->mass_dot_RLOF);
                //printf("0 dt_temp %g *dt_binary_evolution %g\n",dt_temp,*dt_binary_evolution);
                *dt_binary_evolution = min(*dt_binary_evolution, dt_temp);
                //printf("1 dt_temp %g *dt_binary_evolution %g\n",dt_temp,*dt_binary_evolution);
                *dt_binary_evolution = max(*dt_binary_evolution, min_dt);
                //printf("2 dt_temp %g *dt_binary_evolution %g\n",dt_temp,*dt_binary_evolution);
                //printf("fm %g M_dot %g dt_MT %g \n",fm,donor->mass_dot_RLOF,*dt_binary_evolution);
                #ifdef VERBOSE
                printf("fm %g M_dot %g dt_MT %g \n",fm,donor->mass_dot_RLOF,*dt_binary_evolution);
                #endif
    //printf("e %g x %g E_0 %g m_d %g m_a %g fm %g md %g\n",e,x,E_0,M_d,M_a,fm,M_d_dot_av);
                
                #endif
                
            }
        }

    }

    update_structure(particlesMap);
    
    return 0;
    
}

double compute_q_crit_for_common_envelope_evolution(int kw, double mass, double core_mass)
{
    double q_crit;
    
    if (kw == 2)
    {
        q_crit = 4.0;
    }
    else if (kw == 3 or kw == 5 or kw == 6) // Hjellming & Webbink, 1987, ApJ, 318, 794. 
    {
        q_crit = 0.362 + 1.0/(3.0 * (1.0 - core_mass/mass));
    }
    else if (kw == 8 or kw == 9)
    {
        q_crit = 0.784;
    }
    else
    {
        q_crit = 3.0;
    }
    
    return q_crit;
}


int handle_mass_transfer_cases(ParticlesMap *particlesMap, int parent_index, int donor_index, int accretor_index, int *integration_flag, double t_old, double t, double *dt_binary_evolution)
{
    Particle *parent = (*particlesMap)[parent_index];
    Particle *donor = (*particlesMap)[donor_index];
    Particle *accretor = (*particlesMap)[accretor_index];
    
    int flag = -1;
    
    double P_orb = compute_orbital_period(parent);
    double M_donor = donor->mass;
    double M_accretor = accretor->mass;

    /* MT & dynamical timescales */
    double fm = donor->fm;
    double m_dot = compute_orbit_averaged_mass_transfer_rate_emt_model(M_donor,fm,P_orb);
    double t_MT = M_donor/fabs(m_dot);
    double R_donor = donor->radius;
    double t_dyn_donor = compute_stellar_dynamical_timescale(M_donor, R_donor);

    /* Critical mass ratios. */
    double q = M_donor / M_accretor;
    double q_crit = compute_q_crit_for_common_envelope_evolution(donor->stellar_type, M_donor, donor->core_mass);
    double q_crit_low_mass_donor = 0.695;
    double q_crit_WD_donor = 0.628;
    
    int kw = donor->stellar_type;

    /* Different cases */
    if (kw == 0 and q > q_crit_low_mass_donor)
    {
        /* `CE'-like evolution from a low-mass MS star to any secondary. */
        flag = 1;
        dynamical_mass_transfer_low_mass_donor(particlesMap, parent->index, donor->index, accretor->index, t_old, t, integration_flag);
    }
    else if ( (kw >= 2 and kw <= 9 and kw != 7) and ((t_MT <= t_dyn_donor or q > q_crit) or t_MT < P_orb) ) /* add donor convective envelope mass? */
    {
        /* `Standard CE evolution. */
        flag = 2;
        common_envelope_evolution(particlesMap, parent->index, donor->index, accretor->index, integration_flag);
    }
    else if (kw >= 10 and kw <= 12 and q > q_crit_WD_donor)
    {
        /* Dynamical transfer from WD */
        flag = 3;
        dynamical_mass_transfer_WD_donor(particlesMap, parent->index, donor->index, accretor->index, t_old, t, integration_flag);
    }   
    else if (kw == 13 or kw == 14)
    {
        flag = 4;
        mass_transfer_NS_BH_donor(particlesMap, parent->index, donor->index, accretor->index, t_old, t, integration_flag);
    }
    else
    {
        /* Other cases: stable mass transfer. */
        flag = 5;
        stable_mass_transfer_evolution(particlesMap, parent->index, donor->index, accretor->index, t_old, t, integration_flag);
    }
    /* TO DO: contact cases! */
    
    if (flag == -1)
    {
        printf("binary_evolution.cpp -- fatal error in handle_mass_transfer_cases, parent %d donor %d accretor %d\n",parent->index,donor->index,accretor->index);
        exit(-1);
    }
    
    return flag;
}

double compute_orbit_averaged_mass_transfer_rate_emt_model(double M_donor, double fm, double P_orb)
{
    return -(M_donor/P_orb) * fm;
}

double compute_stellar_dynamical_timescale(double M, double R)
{
    return sqrt( ( R*R*R )/(CONST_G * M) );
}


int dynamical_mass_transfer_low_mass_donor(ParticlesMap *particlesMap, int parent_index, int donor_index, int accretor_index, double t_old, double t, int *integration_flag)
{
    /* Low-mass dynamical mass transfer case similar to CE (BSE evolv2.f: lines 1150 - 1233).
     * Always leads to the destruction of the low-mass MS donor; the accretor always survives. 
     * This may not be conservative; any mass not accreted will be assumed to be lost in an isotropic wind. 
     * The latter is computed by adjusting the wind mass loss of the accretor (effectively, mass it cannot accrete is lost in the wind). */
    
    Particle *parent = (*particlesMap)[parent_index];
    Particle *donor = (*particlesMap)[donor_index];
    Particle *accretor = (*particlesMap)[accretor_index];

    /* This type of (fast) evolution is not handled by the ODE integrator; consequently, mass_dot_RLOF should be set to zero. */
    donor->mass_dot_RLOF = 0.0;
    accretor->mass_dot_RLOF = 0.0;
    
    double P_orb = compute_orbital_period(parent);
    double m_donor = donor->mass;
    double m_accretor = accretor->mass;
    double m_accretor_new;
    
    double R_donor = donor->radius;
    double R_accretor = accretor->radius;

    double m_old = m_donor + m_accretor;

    //double fm = donor->fm;
    //double m_dot = compute_orbit_averaged_mass_transfer_rate_emt_model(m_donor,fm,P_orb);
    
    double dm1; /* Mass lost from companion due to RLOF */
    double dm2; /* Mass accreted by companion. */
    
    double dt = t - t_old;
    //dm1 = m_donor;
    
    int kw1 = donor->stellar_type;
    int kw2 = accretor->stellar_type;
    double t_KH_donor = compute_Kelvin_Helmholtz_timescale(kw1,m_donor,donor->core_mass,R_donor,donor->luminosity);
    double t_KH_accretor = compute_Kelvin_Helmholtz_timescale(kw2,m_accretor,accretor->core_mass,R_accretor,accretor->luminosity);

    double t_dyn_donor = compute_stellar_dynamical_timescale(m_donor, R_donor);
    double tau_donor = sqrt(t_KH_donor * t_dyn_donor);
    
    dm1 = -m_donor;

    /* Run star_() on accretor */
    double *GB,*tscls,*tscls_new,*lums,*zpars;
    GB = new double[10];
    tscls = new double[20];
    tscls_new = new double[20];
    lums = new double[10];
    zpars = accretor->zpars;
    double tms,tn;
    star_(&kw2,&accretor->sse_initial_mass,&accretor->mass,&tms,&tn,tscls,lums,GB,zpars); /* note: time units from star_ are in Myr */
    
    double tms_new,tn_new;
        
    if (kw2 <= 1)
    {
        /* Restrict accretion to thermal timescale of secondary. */

        dm2 = -(tau_donor/t_KH_accretor) * dm1;
        m_accretor_new = accretor->mass + dm2;

        /* Rejuvenate if the star is still on the main sequence. */
        accretor->sse_initial_mass = accretor->mass;
        star_(&kw2,&accretor->sse_initial_mass,&m_accretor_new,&tms_new,&tn_new,tscls_new,lums,GB,zpars); 
        //tms_new *= Myr_to_yr;

        /* If the star has no convective core then the effective age decreases,
         * otherwise it will become younger still. */
         
        if (m_accretor_new < 0.35 or m_accretor_new > 1.25)
        {
            accretor->age = (tms_new/tms) * accretor->age * (m_accretor_new - dm2)/m_accretor_new; 
        }
        else
        {
            accretor->age = (tms_new/tms) * accretor->age;
        }
        accretor->epoch = t - accretor->age;
    }
    else if (kw2 <= 6)
    {
        /* Add all the material to the giant's envelope. */

        dm2 = -dm1;
        m_accretor_new = accretor->mass + dm2;
        
        if (kw2 == 2)
        {
            accretor->sse_initial_mass = accretor->mass;
//            double tbgb = tscls[0] * Myr_to_yr; /* old base of GB timescale */
            star_(&kw2,&accretor->sse_initial_mass,&m_accretor_new,&tms_new,&tn_new,tscls_new,lums,GB,zpars); /* will update tscls */
//            tms_new *= Myr_to_yr;
  //          double tbgb_new = tscls[0] * Myr_to_yr;
            
            accretor->age = tms_new*Myr_to_yr + (accretor->age - tms*Myr_to_yr) * (tscls_new[0] / tscls[0]);
            accretor->epoch = t - accretor->age;
        }
    }
    else if (kw2 <= 12)
    {
        /* Form a new giant envelope. */

        dm2 = -dm1;
        m_accretor_new = accretor->mass + dm2;
        int kw = determine_merger_type(kw1,kw2);
        
        if (kw == 4)
        {
            accretor->age = accretor->age / (tms*Myr_to_yr);
            accretor->core_mass = accretor->mass;
        }
        
        /* Check for planets or low-mass WDs. */
        if ((kw2 == 10 and m_accretor < 0.05) or (kw2 >= 11 and m_accretor < 0.5))
        {
            kw = kw1;
        }
        else
        {
            double new_age;
            gntage_(&accretor->core_mass,&m_accretor_new,&kw,zpars,&accretor->sse_initial_mass,&new_age);
            accretor->age = new_age*Myr_to_yr;
            accretor->epoch = t - accretor->age;
        }
    }
    else
    {
        /* The neutron star or black hole simply accretes at (at most) the Eddington rate. */

        double hydrogen_mass_fraction = zpars[10];
        double m_dot_Eddington = compute_Eddington_accretion_rate(R_accretor, hydrogen_mass_fraction);
        double dme = m_dot_Eddington * dt;
       
        dm2 = min(dme*tau_donor/dt, -dm1);
        m_accretor_new = accretor->mass + dm2;
    }
    
    donor->apply_kick = false;
    accretor->apply_kick = false;
    
//    binary->mass = star1->mass + star2->mass; // the old mass -- is this necessary to set?
//    binary->apply_kick = false;
    
    double Delta_m1 = m_donor;
    double Delta_m2 = dm2;
    handle_instantaneous_and_adiabatic_mass_changes_in_orbit(particlesMap, donor, accretor, Delta_m1, Delta_m2, parent->dynamical_mass_transfer_low_mass_donor_timescale, integration_flag); /* TO DO: should use different ML timescale here? */

    /* The binary becomes a body with the accretor's properties. */
    update_stellar_evolution_properties(accretor);
    parent->is_binary = false; 
    copy_all_body_properties(accretor, parent);

    particlesMap->erase(donor_index);
    particlesMap->erase(accretor_index);

    #ifdef IGNORE
   /* Handle effect of mass loss and kicks on orbits in the rest of the system */
      
    
    parent->mass = m_old; // the old total mass
    parent->instantaneous_perturbation_delta_mass = m_new - m_old; /* new mass minus old one; note: if destroyed, the new mass will be 0 */
    parent->instantaneous_perturbation_delta_VX = v_kick_vec[0];
    parent->instantaneous_perturbation_delta_VY = v_kick_vec[1];
    parent->instantaneous_perturbation_delta_VZ = v_kick_vec[2];

    printf("binary_evolution -- dynamical_mass_transfer_WD_donor -- delta m %g v_kick_vec %g %g %g\n",parent->instantaneous_perturbation_delta_mass,v_kick_vec[0],v_kick_vec[1],v_kick_vec[2]);
    apply_instantaneous_mass_changes_and_kicks(particlesMap, integration_flag); /* this will update the binary's mass */
    #endif
    
    
    return 0;
}

double compute_Kelvin_Helmholtz_timescale(int kw, double mass, double core_mass, double radius, double luminosity)
{
    double t_KH = CONST_G * mass / ( 2.0 * radius * luminosity); /* per unit mass */
    if (kw <= 1 or kw == 7 or kw >= 10)
    {
        t_KH *= mass;
    }
    else
    {
        t_KH *= (mass - core_mass);
    }

    return t_KH;
}

double compute_Eddington_accretion_rate(double radius, double hydrogen_mass_fraction)
{
    double kappa = 0.2 * (1.0 + hydrogen_mass_fraction) * 8.88021e6; /* electron scattering opacity in units of AU^2/MSun */
    return eddington_accretion_factor * 4.0 * M_PI * CONST_C_LIGHT * radius/kappa;
}

int dynamical_mass_transfer_WD_donor(ParticlesMap *particlesMap, int parent_index, int donor_index, int accretor_index, double t_old, double t, int *integration_flag)
{
    /* Dynamical mass transfer case WD donor (BSE evolv2.f: lines 1290 - 1342).
     * Always leads to the destruction of the donor; the accretor might survive. 
     * Take into account mass loss from the system, and assume there is no kick imparted on the accretor if it survives. */
    
    Particle *parent = (*particlesMap)[parent_index];
    Particle *donor = (*particlesMap)[donor_index];
    Particle *accretor = (*particlesMap)[accretor_index];

    /* This type of (fast) evolution is not handled by the ODE integrator; correspondingly, mass_dot_RLOF should be set to zero. */
    donor->mass_dot_RLOF = 0.0;
    accretor->mass_dot_RLOF = 0.0;

    double P_orb = compute_orbital_period(parent);
    double m_donor = donor->mass;
    double m_accretor = accretor->mass;

    double m_old = m_donor + m_accretor;

    //double fm = donor->fm;
    //double m_dot = compute_orbit_averaged_mass_transfer_rate_emt_model(m_donor,fm,P_orb);
    
    double dm1; /* Mass lost from companion due to RLOF (<0) */
    double dm2; /* Mass accreted by companion (>0).  */

    double dt = t - t_old;
    //dm1 = m_dot * dt;
    dm1 = -m_donor;
    
    //double dme = 2.08d-03*eddfac*(1.d0/(1.d0 + zpars(11)))*rad(j2)*tb // TO DO: include case for eddfac<10
    
    dm2 = -dm1;

    //double m_new; /* New mass of remnant (if there is one). */
    bool destroyed;;
    double v_kick_vec[3] = {0.0,0.0,0.0};
    
    int kw1 = donor->stellar_type;
    int kw2 = accretor->stellar_type;
    
    if (kw1 == 10 and kw2 == 10)
    {
        /* Assume the energy released by ignition of the triple-alpha reaction is enough to destroy both stars. */
        destroyed = true;
        //m_new = 0.0;
    }
    else if (kw2 >= 10 and kw2 <= 11 and m_accretor > chandrasekhar_mass)
    {
        /* Potentially SNe Ia that destroys the system. */
        destroyed = true;
        //m_new = 0.0;
    }
    else if (kw1 == 10 or kw2 == 10)
    {
        /* Should be helium overflowing onto a CO or ONe core in which case the 
        *  helium swells up to form a giant envelope so a HeGB star is formed. 
        *  Allowance for the rare case of CO or ONe flowing onto He is made. */

        int kst = 9;
        double accretor_core_mass = accretor->core_mass; 
        if (kw2 == 10)
        {
            accretor_core_mass = dm2;
        }
        double accretor_age = accretor->age*yr_to_Myr;
        gntage_(&accretor_core_mass,&m_accretor,&kst,accretor->zpars,&accretor->sse_initial_mass,&accretor_age);
        accretor->stellar_type = kst;
        accretor->epoch = t - accretor_age * Myr_to_yr;
        
        destroyed = false;
        //m_new = m_accretor;
    }
    else if (kw2 <= 12)
    {
        accretor->sse_initial_mass = m_accretor;
        if (kw1 == 12 and kw2 == 11) /* Mixture of ONe and CO will result in an ONe product. */
        {
            accretor->stellar_type = 12;
        }
        destroyed = false;
        //m_new = m_accretor;        
    }
    else
    {
        printf("binary_evolution.cpp -- dynamical_mass_transfer_WD_donor -- unknown case -- parent %d donor %d accretor %d\n",parent_index,donor_index,accretor_index);
        exit(-1);
    }
    
    double Delta_m1 = dm1;
    double Delta_m2 = dm2;
    handle_instantaneous_and_adiabatic_mass_changes_in_orbit(particlesMap, donor, accretor, Delta_m1, Delta_m2, parent->dynamical_mass_transfer_WD_donor_timescale, integration_flag); /* TO DO: should use different ML timescale here? */

    #ifdef IGNORE
   /* Handle effect of mass loss and kicks on orbits in the rest of the system */
      
    
    parent->mass = m_old; // the old total mass
    parent->instantaneous_perturbation_delta_mass = m_new - m_old; /* new mass minus old one; note: if destroyed, the new mass will be 0 */
    parent->instantaneous_perturbation_delta_VX = v_kick_vec[0];
    parent->instantaneous_perturbation_delta_VY = v_kick_vec[1];
    parent->instantaneous_perturbation_delta_VZ = v_kick_vec[2];

    printf("binary_evolution -- dynamical_mass_transfer_WD_donor -- delta m %g v_kick_vec %g %g %g\n",parent->instantaneous_perturbation_delta_mass,v_kick_vec[0],v_kick_vec[1],v_kick_vec[2]);
    apply_instantaneous_mass_changes_and_kicks(particlesMap, integration_flag); /* this will update the binary's mass */
    #endif

    particlesMap->erase(donor_index);
    
    if (destroyed == false)
    {
        /* The binary becomes a body with the accretor's properties. */
        update_stellar_evolution_properties(accretor);
        parent->is_binary = false; 
        copy_all_body_properties(accretor, parent);

        particlesMap->erase(donor_index);
        particlesMap->erase(accretor_index);
    }
    else
    {
        handle_destruction_of_binary_in_system(particlesMap,parent);
    }
    
    return 0;
    
}

int mass_transfer_NS_BH_donor(ParticlesMap *particlesMap, int parent_index, int donor_index, int accretor_index, double t_old, double t, int *integration_flag)
{
    /* "Mass transfer" case NS/BH donor (BSE evolv2.f: lines 1343 - 1365 ) */

    Particle *parent = (*particlesMap)[parent_index];
    Particle *donor = (*particlesMap)[donor_index];
    Particle *accretor = (*particlesMap)[accretor_index];

    /* This type of (fast) evolution is not handled by the ODE integrator; correspondingly, mass_dot_RLOF should be set to zero. */
    donor->mass_dot_RLOF = 0.0;
    accretor->mass_dot_RLOF = 0.0;

    collision_product(particlesMap, parent_index, donor_index, accretor_index, integration_flag);

    return 0;
}


int common_envelope_evolution(ParticlesMap *particlesMap, int binary_index, int index1, int index2, int *integration_flag)
{
    /*
     * **
      SUBROUTINE COMENV(M01,M1,MC1,AJ1,JSPIN1,KW1,
     &                  M02,M2,MC2,AJ2,JSPIN2,KW2,
     &                  ZPARS,ECC,SEP,JORB,COEL)
*
* Common Envelope Evolution.
* Mostly copied from BSE.
*
*     Author : C. A. Tout
*     Date :   18th September 1996
*
*     Redone : J. R. Hurley
*     Date :   7th July 1998
*
* Note units in SSE/BSE: length in RSUN, time in Myr, luminosity in LSun
*/

    printf("CE binary_index %d index1 %d index2 %d\n",binary_index,index1,index2);
    int i;

    Particle *star1 = (*particlesMap)[index1];
    Particle *star2 = (*particlesMap)[index2];
    double M1 = star1->mass;
    double M2 = star2->mass;

    /* CE evolution is not handled by the ODE integrator; correspondingly, mass_dot_RLOF should be set to zero. */
    star1->mass_dot_RLOF = 0.0;
    star2->mass_dot_RLOF = 0.0;

    /* Orbital properties */
    double SEP;
    double SEPF; // final separation
    double SEPL;
    double ECC;

    Particle *binary;
    double h_vec_unit[3],e_vec_unit[3];
    double initial_momentum[3];    
    if (*integration_flag == 0) /* secular mode */
    {
        set_up_derived_quantities(particlesMap); /* for setting a, e, etc. */
        binary = (*particlesMap)[binary_index];
        SEP = binary->a/CONST_R_SUN; // initial separation
        ECC = binary->e;
        get_unit_vector(binary->h_vec,h_vec_unit);
        get_unit_vector(binary->e_vec,e_vec_unit);
    }
    else /* Direct N-body mode */
    {
        double h_vec[3],e_vec[3],r[3],v[3];
        double true_anomaly;
        
        for (i=0; i<3; i++)
        {
            r[i] = star1->R_vec[i] - star2->R_vec[i];
            v[i] = star1->V_vec[i] - star2->V_vec[i];
            initial_momentum[i] = M1 * star1->V_vec[i] + M2 * star2->V_vec[i];
        }
        from_cartesian_to_orbital_vectors(M1,M2,r,v,e_vec,h_vec,&true_anomaly);
        ECC = norm3(e_vec);
        SEP = compute_a_from_h(M1,M2,norm3(h_vec),ECC);
        get_unit_vector(h_vec,h_vec_unit);
        get_unit_vector(e_vec,e_vec_unit);
        printf("CE Integration_flag>0 a %g e %g\n",SEP,ECC);
    }

    int CEFLAG = binary_evolution_CE_energy_flag;
    double ALPHA1 = binary->common_envelope_alpha;
    
    double fac = 1.0;
        
    bool COEL = false;
    
    /* Get current state of star 1 (primary) */
    double TM1,TN1;
    double *GB1,*TSCLS1,*LUMS1;
    GB1 = new double[10];
    TSCLS1 = new double[20];
    LUMS1 = new double[10];    
    
    int KW1 = star1->stellar_type;
    double M01 = star1->sse_initial_mass;
    double MC1 = star1->core_mass;
    double R1 = star1->radius/CONST_R_SUN;
    double RC1 = star1->core_radius/CONST_R_SUN;
    double L1 = star1->luminosity/CONST_L_SUN;
    double MENV1,RENV1,K21;
    //double K21;
    
    double AJ1 = star1->age * yr_to_Myr;
    double *ZPARS1 = star1->zpars;    
    //printf("t1 %g %d %g %g %g %g\n",ZPARS1[0],KW1,M01,M1,TM1,TN1);
    star_(&KW1, &M01, &M1, &TM1, &TN1, TSCLS1, LUMS1, GB1, ZPARS1);
    //printf("t2 %g %d %g %g %g %g\n",ZPARS1[0],KW1,M01,M1,TM1,TN1);
    //printf("arg hrd %g %g %g %g %g \n",M01,AJ1,M1,TM1,TN1);
    hrdiag_(&M01,&AJ1,&M1,&TM1,&TN1,TSCLS1,LUMS1,GB1,ZPARS1, \
        &R1,&L1,&KW1,&MC1,&RC1,&MENV1,&RENV1,&K21);

//      OSPIN1 = JSPIN1/(K21*R1*R1*(M1-MC1)+K3*RC1*RC1*MC1)
    double MENVD1 = MENV1 / (M1 - MC1);
    double RZAMS1 = rzamsf_(&M01);
    double LAMB1 = celamf_(&KW1,&M01,&L1,&R1,&RZAMS1,&MENVD1,&fac);

    /* Get current state of star 2 (secondary) */
    double TM2,TN2;
    double *GB2,*TSCLS2,*LUMS2;
    GB2 = new double[10];
    TSCLS2 = new double[20];
    LUMS2 = new double[10];    
    
    int KW2 = star2->stellar_type;
    double M02 = star2->sse_initial_mass;
    double MC2 = star2->core_mass;
    double R2 = star2->radius/CONST_R_SUN;
    double RC2 = star2->core_radius/CONST_R_SUN;
    double L2 = star2->luminosity/CONST_L_SUN;
    double MENV2,RENV2,K22;
    //double K22;
    
    double AJ2 = star2->age * yr_to_Myr;
    double *ZPARS2 = star2->zpars;    
    //printf("t3 %g %d %g %g %g %g\n",ZPARS2[0],KW2,M02,M2,TM2,TN2);
    star_(&KW2, &M02, &M2, &TM2, &TN2, TSCLS2, LUMS2, GB2, ZPARS2);
    //printf("t4 %g %d %g %g %g %g\n",ZPARS2[0],KW2,M02,M2,TM2,TN2);    
    
    hrdiag_(&M02,&AJ2,&M2,&TM2,&TN2,TSCLS2,LUMS2,GB2,ZPARS2, \
        &R2,&L2,&KW2,&MC2,&RC2,&MENV2,&RENV2,&K22);
//      OSPIN1 = JSPIN1/(K21*R1*R1*(M1-MC1)+K3*RC1*RC1*MC1)
    double MENVD2 = MENV2 / (M2 - MC2);
    double RZAMS2 = rzamsf_(&M02);
    double LAMB2 = celamf_(&KW2,&M02,&L2,&R2,&RZAMS2,&MENVD2,&fac);


    /*
    * Calculate the binding energy of the primary star's envelope (multiplied by lambda).
    * Also, calculate initial orbital energy.
    * Energies are divided by -G.
    */

    double EBINDI = M1 * (M1 - MC1)/(LAMB1 * R1);
    double EORBI = M1 * M2/(2.0 * SEP); // use primary total mass and secondary total mass (HTP02 eq. 69)

    /*
    * If the secondary star is also giant-like, add its envelope's energy.
    */
    
    if (KW2 >= 2 and KW2 <= 9 and KW2 != 7)
    {
        EBINDI += M2 * (M2 - MC2) / (LAMB2 * R2);

        if(CEFLAG != 3) // use primary core mass instead of total mass
        {
            EORBI = MC1 * MC2/(2.0 * SEP); // secondary is giant; use its core mass
        }
    }
    else
    {
        if(CEFLAG !=  3) // use primary core mass instead of total mass
        {
            EORBI = MC1 * M2/(2.0 * SEP); // secondary is not a giant; use its total mass
        }
    }
    
    double ECIRC = EORBI/(1.0 - ECC*ECC); // Allow for an eccentric orbit.

    double EORBF = EORBI + EBINDI/ALPHA1; // Calculate the final orbital energy without coalescence (HTP02 eq. 71).
    double EBINDF;
    
    /* Generic variables */

    double TN;
    double *GB,*TSCLS,*LUMS;
    GB = new double[10];
    TSCLS = new double[20];
    LUMS = new double[10];    
    double MENV,RENV;

    
    /* Variables for potentially merged stars */
    int KW;
    double MC3;
    double MF;
    
    double TB,OORB;
    double XX,CONST;
    double DELY,DERI,DELMF; // used for the NR iteration procedure
    bool iterate = true;

    
    /* By default, do not apply any kicks; can be changed below depending on situation 
     * Note: apply_kick will be set back to the default true at the end below */
    star1->apply_kick = false;
    star1->instantaneous_perturbation_delta_mass = 0.0;
    
    star2->apply_kick = false;
    star2->instantaneous_perturbation_delta_mass = 0.0;

    
    if(KW2 <= 1 or KW2 == 7)
    {
        /*
        * If the secondary is on the main sequence, see if it fills its Roche lobe.
        */
        
        SEPF = MC1 * M2/(2.0 * EORBF); // secondary does not have a core; assume it did not lose mass
        double Q1 = MC1 / M2;
        double Q2 = 1.0 / Q1;
         
        double RL1 = roche_radius_pericenter_eggleton(SEP,Q1) / SEP; // dimensionless RL radius (SEP cancels out)
        double RL2 = roche_radius_pericenter_eggleton(SEP,Q2) / SEP; // dimensionless RL radius (SEP cancels out)

        if (RC1/RL1 >= R2/RL2)
        {

            /* The helium core of a very massive star of type 4 may actually fill
            * its Roche lobe in a wider orbit with a very low-mass secondary. */
            
            if (RC1 > RL1 * SEPF)
            {
               COEL = true;
               SEPL = RC1 / RL1; // separation at which the stars coelesced
            }
        }
        else
        {
            if (R2 > RL2 * SEPF)
            {
               COEL = true;
               SEPL = R2 / RL2;
            }
        }

        if (COEL == true) /* Coalescence - calculate final binding energy. */
        {
            //KW = KTYPE(KW1,KW2) - 100
            KW = determine_merger_type(KW1,KW2);
            MC3 = MC1;
            if (KW2 == 7 and KW == 4)
            {
                MC3 = MC1 + M2; // note ASH: replaced `MC3' in BSE code to `MC1' -- does not change anything functionally, but is more clear
            }
            
            EORBF = max(MC1 * M2/(2.0 * SEPL) , EORBI);
            EBINDF = EBINDI - ALPHA1 * (EORBF - EORBI);
        }
        else // Primary becomes a black hole, neutron star, white dwarf or helium star.
        {
            MF = M1;
            M1 = MC1;
            star_(&KW1,&M01,&M1,&TM1,&TN,TSCLS1,LUMS,GB,ZPARS1);
            hrdiag_(&M01,&AJ1,&M1,&TM1,&TN,TSCLS1,LUMS,GB,ZPARS1, \
                &R1,&L1,&KW1,&MC1,&RC1,&MENV1,&RENV1,&K21);
            
            if (KW1 >= 13)
            {
                star1->apply_kick = true;
                star1->kick_distribution = 1;
                //star1->instantaneous_perturbation_delta_mass = M1 - MF;
            }
            
            // TO DO: include kicks 
            //IF(KW1.GE.13)THEN
               //CALL kick(KW1,MF,M1,M2,ECC,SEPF,JORB,VS)
               //IF(ECC.GT.1.D0) GOTO 30
            //ENDIF
         }
    }
    else
    {
        /*
        * Degenerate or giant secondary. Check if the least massive core fills its Roche lobe.
        */

        SEPF = MC1 * MC2/(2.0 * EORBF);
        double Q1 = MC1 / MC2;
        double Q2 = 1.0 / Q1;
        
        double RL1 = roche_radius_pericenter_eggleton(SEP,Q1) / SEP; // dimensionless RL radius (SEP cancels out)
        double RL2 = roche_radius_pericenter_eggleton(SEP,Q2) / SEP; // dimensionless RL radius (SEP cancels out)
        
        if (RC1/RL1 >= RC2/RL2)
        {
            if (RC1 > RL1 * SEPF)
            {
                COEL = true;
                SEPL = RC1 / RL1;
            }
        }
        else
        {
            if (RC2 > RL2 * SEPF)
            {
                COEL = true;
                SEPL = RC2 / RL2;
            }
        }

        if (COEL == true)
        {
            
            SEPF = 0.0; // ASH: redundant?
            if(KW2 >= 13)
            {
            /* If the secondary was a neutron star or black hole the outcome
            * is an unstable Thorne-Zytkow object that leaves only the core. */

               MC1 = MC2;
               M1 = MC1;
               MC2 = 0.0;
               M2 = 0.0;
               KW1 = KW2;
               KW2 = 15;
               AJ1 = 0.0;
        //* The envelope mass is not required in this case.
        //*
               goto label30; // TO DO: proper handling
            }
            //ENDIF
            //else // secondary was NOT an NS/BH
                
            //KW = KTYPE(KW1,KW2) - 100
            KW = determine_merger_type(KW1,KW2);
            MC3 = MC1 + MC2;

            /*
            * Calculate the final envelope binding energy.
            */
            
            EORBF = max(MC1 * MC2/(2.0 * SEPL), EORBI);
            EBINDF = EBINDI - ALPHA1*(EORBF - EORBI);

            /*
            * Check if we have the merging of two degenerate cores and if so
            * then see if the resulting core will survive or change form.
            */
            
            if (KW1 == 6 and (KW2 == 6 or KW2 >= 11))
            {
               dgcore_(&KW1,&KW2,&KW,&MC1,&MC2,&MC3,&EBINDF);
            }

            if(KW1 <= 3 and M01 <= ZPARS1[1])
            {
                if ( (KW2 >= 2 and KW2 <= 3 and M02 <= ZPARS2[1]) or KW2 == 10)
                {
                    dgcore_(&KW1,&KW2,&KW,&MC1,&MC2,&MC3,&EBINDF);
                    if(KW >= 10) // new star is compact object
                    {
                        KW1 = KW;
                        M1 = MC3;
                        MC1 = MC3;
                        if (KW < 15)
                        {
                            M01 = MC3;
                        }
                        AJ1 = 0.0;
                        MC2 = 0.0;
                        M2 = 0.0;
                        KW2 = 15;
                        
                        goto label30;
                        //GOTO 30 // TO DO: proper handling
                    }
                }
            }
        }
        else
        {
            
            /*
            * The cores do not coalesce - assign the correct masses and ages.
            */
            
            MF = M1;
            M1 = MC1;
            star_(&KW1,&M01,&M1,&TM1,&TN,TSCLS1,LUMS,GB,ZPARS1);
            hrdiag_(&M01,&AJ1,&M1,&TM1,&TN,TSCLS1,LUMS,GB,ZPARS1, \
                &R1,&L1,&KW1,&MC1,&RC1,&MENV1,&RENV1,&K21);
            if (KW1 >= 13)
            {
                star1->apply_kick = true;
                star1->kick_distribution = 1;
                // TO DO: handle kick
               //CALL kick(KW1,MF,M1,M2,ECC,SEPF,JORB,VS)
               //IF(ECC.GT.1.D0) GOTO 30
            }
            
            MF = M2;
            KW = KW2;
            M2 = MC2;
            star_(&KW2,&M02,&M2,&TM2,&TN,TSCLS2,LUMS,GB,ZPARS2);
            hrdiag_(&M02,&AJ2,&M2,&TM2,&TN,TSCLS2,LUMS,GB,ZPARS2, \
                &R2,&L2,&KW2,&MC2,&RC2,&MENV2,&RENV2,&K22);
            if(KW2 >= 13 and KW < 13) /* secondary became an NS */
            {
                star2->apply_kick = true;
                star2->kick_distribution = 1;
                // TO DO: handle kick
               //CALL kick(KW2,MF,M2,M1,ECC,SEPF,JORB,VS)
               //IF(ECC.GT.1.D0) GOTO 30
            }
            
        }
    } // back at main level
    
    double MC22; // effective core mass of the secondary
    double FAGE1,FAGE2;


    /* If making a single helium burning star, calculate the fractional age 
     * depending on the amount of helium that has burnt. */
    if (COEL == true)
    {
        MC22 = MC2;
        if(KW == 4 or KW == 7)
        {
            if (KW1 <= 3)
            {
               FAGE1 = 0.0;
            }
            else if (KW1 >= 6)
            {
               FAGE1 = 1.0;
            }
            else
            {
               FAGE1 = (AJ1 - TSCLS1[1])/(TSCLS1[12] - TSCLS1[1]);
            }

            if (KW2 <= 3 or KW2 == 10)
            {
               FAGE2 = 0.0;
            }
            else if (KW2 == 7)
            {
               FAGE2 = AJ2/TM2;
               MC22 = M2;
            }
            else if (KW2 >= 6)
            {
               FAGE2 = 1.0;
            }
            else
            {
               FAGE2 = (AJ2 - TSCLS2[1])/(TSCLS2[12] - TSCLS2[1]);
            }
        }
        //printf("COEL True FAGE1 %g FAGE 2 %g\n",FAGE1,FAGE2);
    }

    /* Now calculate the final mass following coelescence.  This requires a Newton-Raphson iteration. 
    * Note ASH: as a temporary measure, taking metallicity of star2 for the coelesced object */

    double z_new;
    double *zpars_new;
    zpars_new = new double[20];

    if (COEL == true)
    {
        z_new = star2->metallicity;
        zcnsts_(&z_new,zpars_new);

        /* Calculate the orbital spin just before coalescence. */
         //TB = (SEPL/AURSUN)*SQRT(SEPL/(AURSUN*(MC1+MC2)))
        TB = TWOPI * (SEPL * CONST_R_SUN) * sqrt( SEPL * CONST_R_SUN / (CONST_G * (MC1 + MC2) ) );
        OORB = TWOPI/TB;

        /* Set up Newton-Raphson iteration */
        XX = 1.0 + ZPARS2[6];

        if(EBINDF <= 0.0)
        {
           MF = MC3;
           iterate = false;
        }
        else
        {
           CONST = pow(M1 + M2, XX) * (M1 - MC1 + M2 - MC22) * (EBINDF/EBINDI);
        }
    
        if (iterate == true)
        {
            /* Initial Guess. */
            MF = max( MC1 + MC22, (M1 + M2) * pow(EBINDF/EBINDI, 1.0/XX) );

            /* Start iteration */
            while(true)
            {
                DELY = pow(MF, XX) * (MF - MC1 - MC22) - CONST;
                
    //            if (fabs( pow(DELY/MF, 1.D0 + XX) <= 1.0e-2)) /* Note ASH: this case is commented out in BSE */
    //            {
    //                break;
    //            }
                if (fabs(DELY/MF) <= 1.0e-3)
                {
                    break;
                }

                DERI = pow(MF, ZPARS2[6]) * ( (1.0 + XX) * MF - XX * (MC1 + MC22) );
                DELMF = DELY/DERI;
                MF = MF - DELMF;
            }
        }
        
        /* Set the masses and separation. */
        if (MC22 == 0.0)
        {
            MF = max(MF, MC1 + M2);
        }
        M2 = 0.0;
        M1 = MF;
        KW2 = 15;

        /* Combine the core masses. */

        if (KW == 2)
        {
            star_(&KW,&M1,&M1,&TM2,&TN,TSCLS2,LUMS,GB,ZPARS2);
            if(GB[8] >= MC1)
            {
               M01 = M1;
               AJ1 = TM2 + (TSCLS2[0] - TM2) * (AJ1 - TM1) / (TSCLS1[0] - TM1);
               star_(&KW,&M01,&M1,&TM1,&TN,TSCLS1,LUMS,GB,ZPARS2);
            }
        }
        else if (KW == 7)
        {
            M01 = M1;
            star_(&KW,&M01,&M1,&TM1,&TN,TSCLS1,LUMS,GB,ZPARS2);
            AJ1 = TM1 * (FAGE1 * MC1 + FAGE2 * MC22) / (MC1 + MC22);
        }
        else if (KW == 4 or MC2 > 0.0 or KW != KW1)
        {
            if (KW == 4)
            {
                AJ1 = (FAGE1 * MC1 + FAGE2 * MC22) / (MC1 + MC22); /* Note ASH: this is actually an age factor (dimensionless); AJ1 will be changed below in gntage_ */
            }
            MC1 = MC1 + MC2;
            MC2 = 0.0;

            /* Obtain a new age for the giant. */

            gntage_(&MC1,&M1,&KW,ZPARS2,&M01,&AJ1);
            star_(&KW,&M01,&M1,&TM1,&TN,TSCLS1,LUMS,GB,ZPARS2);
         }
         
         hrdiag_(&M01,&AJ1,&M1,&TM1,&TN,TSCLS1,LUMS,GB,ZPARS2, \
            &R1,&L1,&KW,&MC1,&RC1,&MENV1,&RENV1,&K21);
         //JSPIN1 = OORB*(K21*R1*R1*(M1-MC1)+K3*RC1*RC1*MC1)
         KW1 = KW;
         ECC = 0.0;
    }
    else if (COEL == false) /* No coalescence */
    {
        
        /* Check if any eccentricity remains in the orbit by first using 
        * energy to circularise the orbit before removing angular momentum. 
        * (note this should not be done in case of CE SN ... fix).  */

         if(EORBF < ECIRC)
         {
            ECC = sqrt(1.0 - EORBF/ECIRC);
         }
         else
         {
            ECC = epsilon;
         }

         /* Set both cores in co-rotation with the orbit on exit of CE */

         //TB = (SEPF/AURSUN)*SQRT(SEPF/(AURSUN*(M1+M2)))
         TB = TWOPI * (SEPF * CONST_R_SUN) * sqrt( SEPF * CONST_R_SUN / (CONST_G * (M1 + M2) ) );
         OORB = TWOPI/TB;
         //double JORB = M1 * M2 / (M1 + M2) * sqrt(1.0 - ECC * ECC) * SEPF * SEPF * OORB;
//*        JSPIN1 = OORB*(K21*R1*R1*(M1-MC1)+K3*RC1*RC1*MC1)
//*        JSPIN2 = OORB*(K22*R2*R2*(M2-MC2)+K3*RC2*RC2*MC2)

        /* or, leave the spins of the cores as they were on entry.
        * Tides will deal with any synchronization later. */

//         JSPIN1 = OSPIN1 * (K21*R1*R1*(M1-MC1)+K3*RC1*RC1*MC1);
//         JSPIN2 = OSPIN2 * (K22*R2*R2*(M2-MC2)+K3*RC2*RC2*MC2);

    }



    /* Handle orbital changes / merge binaries in case of coalescence */
    
    label30:
    {
    
    bool unbound_orbits = false;
    double spin_vec_unit[3];
    printf("COEL %d\n",COEL);
    if (COEL == false)
    {
        
        /* Handle effect of mass loss on the rest of the system */
        printf("no coel M1 %g M2 %g kw1 %d kw2 %d\n",M1,M2,KW1,KW2);

        /* First, handle effects of possible SNe kicks (apply_kick = true for star1 and/or star2). Mass loss is not yet taken into account */
        star1->instantaneous_perturbation_delta_mass = 0.0;
        star2->instantaneous_perturbation_delta_mass = 0.0;
        
        int integration_flag_dummy;
        handle_SNe_in_system(particlesMap, &unbound_orbits, &integration_flag_dummy);

        printf("CE -- post handle_SNe_in_system\n");
        print_system(particlesMap);

        /* Next, handle effects of mass loss */
        binary->mass = star1->mass + star2->mass; // the old mass -- is this necessary to set?
        binary->apply_kick = false;
            
        double Delta_M1 = M1 - star1->mass;
        double Delta_M2 = M2 - star2->mass;
        handle_instantaneous_and_adiabatic_mass_changes_in_orbit(particlesMap, star1, star2, Delta_M1, Delta_M2, binary->common_envelope_timescale, &integration_flag_dummy);

        #ifdef IGNORE
        set_old_parameters_for_adiabatic_mass_loss(particlesMap); /* copy the old masses and h & e vectors for use of adiabatic mass loss below */
        
        /* Next, assume instantaneous mass loss for ALL orbits. 
         * Note: `binary' will be overwritten at the very end below. 
         * Other orbits, for which mass loss is expected to be adiabatic,
         * are adjusted below in compute_new_orbits_assuming_adiabatic_mass_loss. */
        star1->apply_kick = false; /* The kicks were already taken into account above */
        star2->apply_kick = false; /* The kicks were already taken into account above */

        double Delta_M1 = M1 - star1->mass;
        double Delta_M2 = M2 - star2->mass;
        star1->instantaneous_perturbation_delta_mass = Delta_M1; // final minus initial mass
        star2->instantaneous_perturbation_delta_mass = Delta_M2; // final minus initial mass        
        
        handle_SNe_in_system(particlesMap, &unbound_orbits, integration_flag); /* this will update the masses of stars 1 and 2 */
        
        /* Lastly, `correct' the orbits (h & e vectors) for which the effect of mass loss was actually expected to be adiabatic,
         * i.e., time of CE >> t_orb */
        star1->delta_m_adiabatic_mass_loss = Delta_M1;
        star2->delta_m_adiabatic_mass_loss = Delta_M2;
        compute_new_orbits_assuming_adiabatic_mass_loss(particlesMap, binary->common_envelope_timescale);
        #endif
        
        
        //apply_instantaneous_mass_changes_and_kicks(particlesMap, integration_flag);
        printf("CE -- post mass changes\n");
        print_system(particlesMap);
        /* Update the stars and orbit */
        star1->stellar_type = KW1;
        
        //star1->mass = M1; // handled by SNe function
        star1->core_mass = MC1;
        star1->sse_initial_mass = M01;
        star1->convective_envelope_mass = MENV1;
        
        star1->epoch = 0.0; /* is this correct? */
        star1->age = AJ1 * Myr_to_yr;

        star1->radius = R1 * CONST_R_SUN;
        star1->core_radius = RC1 * CONST_R_SUN;
        star1->convective_envelope_radius = RENV1 * CONST_R_SUN;
        
        star1->luminosity = L1 * CONST_L_SUN;
        
        /* Spins: either assume the spins do not change (binary_evolution_CE_spin_flag=0) */
        /* Alternatively: spins corotate with final orbit (binary_evolution_CE_spin_flag=1) */
        if (binary_evolution_CE_spin_flag == 1)
        {
            get_unit_vector(star1->spin_vec,spin_vec_unit);
            for (int i=0; i<3; i++)
            {
                star1->spin_vec[i] = OORB * spin_vec_unit[i];
            }
        }

        star2->stellar_type = KW2;
        
        //star2->mass = M2; // handled by SNe function
        star2->core_mass = MC2;
        star2->sse_initial_mass = M02;
        star2->convective_envelope_mass = MENV2;
        
        star2->epoch = 0.0; /* is this correct? */
        star2->age = AJ2 * Myr_to_yr;

        star2->radius = R2 * CONST_R_SUN;
        star2->core_radius = RC2 * CONST_R_SUN;
        star2->convective_envelope_radius = RENV2 * CONST_R_SUN;
        
        star2->luminosity = L2 * CONST_L_SUN;

        if (binary_evolution_CE_spin_flag == 1)
        {
            get_unit_vector(star2->spin_vec,spin_vec_unit);
            for (int i=0; i<3; i++)
            {
                star2->spin_vec[i] = OORB * spin_vec_unit[i];
            }
        }

        /* Set new e & h vectors for the binary.
         * Assume they only change in magnitude.
         * Note: this overrides the changes made to the binary's orbit after calling handle_SNe_in_system above */

        double a = SEPF * CONST_R_SUN;
        double e = ECC;
        double h = compute_h_from_a(M1,M2,a,e);

        printf("a %g e %g h %g\n",a,e,h);

        if (*integration_flag == 0) /* secular */
        {
            for (int i=0; i<3; i++)
            {
                binary->e_vec[i] = e * binary->e_vec_unit[i];
                binary->h_vec[i] = h * binary->h_vec_unit[i];
            }

            /* Below: not strictly neccesary */
            binary->a = a;
            binary->e = ECC;
        
            /* Assume the binary true anomaly is not affected */
            
            set_binary_masses_from_body_masses(particlesMap);
            set_positions_and_velocities(particlesMap); /* update positions and velocities of all bodies based on updated binary e and h vectors */
        }        
        /* Override the integration flag (cf. handle_instantaneous_and_adiabatic_mass_changes_in_orbit above) */
        bool unbound_orbits = check_for_unbound_orbits(particlesMap);
        if (unbound_orbits == true)
        {
            *integration_flag = 5;
            printf("binary_evolution.cpp -- CE -- Unbound orbits in system; switching to integration_flag %d\n",*integration_flag);
        }
    
    }
    else if (COEL == true)
    {
        if (*integration_flag == 0) /* secular */
        {
            #ifdef IGNORE
            /* Handle effect of mass loss on the rest of the system */

            set_old_parameters_for_adiabatic_mass_loss(particlesMap); /* copy the old masses and h & e vectors for use of adiabatic mass loss below */
            
            /* Next, assume instantaneous mass loss for ALL orbits. 
             * Orbits, for which mass loss is expected to be adiabatic,
             * are adjusted below in compute_new_orbits_assuming_adiabatic_mass_loss. */

            binary->mass = star1->mass + star2->mass; // the old mass
            double Delta_M = M1 - binary->mass;
            binary->instantaneous_perturbation_delta_mass = M1 - binary->mass;

            int integration_flag_dummy;
            handle_SNe_in_system(particlesMap, &unbound_orbits, &integration_flag_dummy); /* mass of `binary' will be updated */

            //apply_instantaneous_mass_changes_and_kicks(particlesMap, integration_flag);
            
            //apply_user_specified_instantaneous_perturbation(particlesMap);
            //reset_instantaneous_perturbation_quantities(particlesMap);
            
            //printf("mold %g mnew %g\n",binary->mass,M1);
            
            /* Lastly, `correct' the orbits (h & e vectors) for which the effect of mass loss was actually expected to be adiabatic,
             * i.e., time of CE >> t_orb */
            binary->delta_m_adiabatic_mass_loss = Delta_M;
            compute_new_orbits_assuming_adiabatic_mass_loss(particlesMap, binary->common_envelope_timescale);

            #endif

            /* Should coelesced objects get a kick? 
             * For now, assume no kick
             * Note: mergers between BHs/NSs (implying possible recoil) should never occur here (CE evolution), rather as collisions */
            star1->apply_kick = false;
            star2->apply_kick = false;
            
            binary->mass = star1->mass + star2->mass; // the old mass -- is this necessary to set again here?
            binary->apply_kick = false;
            
            double Delta_M1 = M1 - binary->mass;
            double Delta_M2 = 0.0;
            int integration_flag_dummy;
            handle_instantaneous_and_adiabatic_mass_changes_in_orbit(particlesMap, star1, star2, Delta_M1, Delta_M2, binary->common_envelope_timescale, &integration_flag_dummy);

            binary->is_binary = false; /* The binary becomes a body */
            particlesMap->erase(star1->index);
            particlesMap->erase(star2->index);

            //printf("post merge\n");
            //print_system(particlesMap);

            binary->apply_kick = false;
            binary->merged = false;
            //printf("done SNe\n");
            
            /* Update the merged star */
            binary->stellar_type = KW1;
            
            //star1->mass = M1; // handled by SNe function
            binary->core_mass = MC1;
            binary->sse_initial_mass = M01;
            binary->convective_envelope_mass = MENV1;
            
            binary->epoch = 0.0; /* is this correct? */
            binary->age = AJ1 * Myr_to_yr;

            binary->radius = R1 * CONST_R_SUN;
            binary->core_radius = RC1 * CONST_R_SUN;
            binary->convective_envelope_radius = RENV1 * CONST_R_SUN;
            
            binary->luminosity = L1 * CONST_L_SUN;
            
            binary->metallicity = z_new;
            binary->zpars = zpars_new;

            /* Set the spin equal to the orbital frequency just before coalescence (previously calculated as OORB).
             * Assume the direction is equal to the previous orbital orientation. */

            for (int i=0; i<3; i++)
            {
                binary->spin_vec[i] = OORB * spin_vec_unit[i];
            }
            
            binary->apply_kick = true; /* Default value for (stellar evolution) bodies */
            
            /* Override the integration flag (cf. handle_instantaneous_and_adiabatic_mass_changes_in_orbit above) */
            bool unbound_orbits = check_for_unbound_orbits(particlesMap);
            if (unbound_orbits == true)
            {
                *integration_flag = 5;
                printf("binary_evolution.cpp -- CE -- Unbound orbits in system; switching to integration_flag %d\n",*integration_flag);
            }
        }
        else
        {
            /* Assign merger remnant to star1; remove star2 */
            particlesMap->erase(star2->index);
            star1->stellar_type = KW1;

            for (i=0; i<3; i++)
            {
                star1->V_vec[i] = initial_momentum[i]/M1; /* set new velocity according to linear momentum conservation */
            }

            star1->mass = M1;
            star1->core_mass = MC1;
            star1->sse_initial_mass = M01;
            star1->convective_envelope_mass = MENV1;
            
            star1->epoch = 0.0; /* is this correct? */
            star1->age = AJ1 * Myr_to_yr;

            star1->radius = R1 * CONST_R_SUN;
            star1->core_radius = RC1 * CONST_R_SUN;
            star1->convective_envelope_radius = RENV1 * CONST_R_SUN;
            
            star1->luminosity = L1 * CONST_L_SUN;
            
            /* Set the spin equal to the orbital frequency just before coalescence (previously calculated as OORB).
             * Assume the direction is equal to the previous orbital orientation. */

            for (int i=0; i<3; i++)
            {
                star1->spin_vec[i] = OORB * spin_vec_unit[i];
            }
            
            star1->apply_kick = true; /* Default value for (stellar evolution) bodies */
        }
    }
    
    /* Reset kick parameters if the binary remains */
    if (COEL == false)
    {
        star1->apply_kick = true;
        star2->apply_kick = true;
    }

    } // end of label30

    return 1;
}

void handle_instantaneous_and_adiabatic_mass_changes_in_orbit(ParticlesMap *particlesMap, Particle *star1, Particle *star2, double Delta_m1, double Delta_m2, double mass_loss_timescale, int *integration_flag)
{

    set_old_parameters_for_adiabatic_mass_loss(particlesMap); /* copy the old masses and h & e vectors for use of adiabatic mass loss below */

    /* Next, assume instantaneous mass loss for ALL orbits. 
     * Note: `binary' will be overwritten at the very end below. 
     * Other orbits, for which mass loss is expected to be adiabatic,
     * are adjusted below in compute_new_orbits_assuming_adiabatic_mass_loss. */
    //star1->apply_kick = false; /* The kicks were already taken into account above */
    //star2->apply_kick = false; /* The kicks were already taken into account above */

    //double Delta_M1 = M1 - star1->mass;
    //double Delta_M2 = M2 - star2->mass;
    star1->instantaneous_perturbation_delta_mass = Delta_m1; // final minus initial mass
    star2->instantaneous_perturbation_delta_mass = Delta_m2; // final minus initial mass        

    //handle_SNe_in_system(particlesMap, &unbound_orbits, integration_flag); /* this will update the masses of stars 1 and 2 */
    apply_instantaneous_mass_changes_and_kicks(particlesMap, integration_flag);
    
    /* Lastly, `correct' the orbits (h & e vectors) for which the effect of mass loss was actually expected to be adiabatic,
     * i.e., time of CE >> t_orb */
    star1->delta_m_adiabatic_mass_loss = Delta_m1;
    star2->delta_m_adiabatic_mass_loss = Delta_m2;
    compute_new_orbits_assuming_adiabatic_mass_loss(particlesMap, mass_loss_timescale);
}

void set_old_parameters_for_adiabatic_mass_loss(ParticlesMap *particlesMap)
{
    int i;
    ParticlesMapIterator it;
    for (it = particlesMap->begin(); it != particlesMap->end(); it++)
    {
        Particle *b = (*it).second;
        b->delta_m_adiabatic_mass_loss = 0.0;
        if (b->is_binary == true)
        {
            b->P_orb_adiabatic_mass_loss = compute_orbital_period(b);
            b->child1_mass_adiabatic_mass_loss = b->child1_mass;
            b->child2_mass_adiabatic_mass_loss = b->child2_mass;
            for (i=0; i<3; i++)
            {
                b->h_vec_old_adiabatic_mass_loss[i] = b->h_vec[i];
                b->e_vec_old_adiabatic_mass_loss[i] = b->e_vec[i];
            }
        }
    }
}

void compute_new_orbits_assuming_adiabatic_mass_loss(ParticlesMap *particlesMap, double mass_loss_timescale)
{
    set_binary_masses_from_body_masses(particlesMap); // to set the correct delta_m_adiabatic for each orbit

    int i;    
    ParticlesMapIterator it;
    for (it = particlesMap->begin(); it != particlesMap->end(); it++)
    {
        Particle *b = (*it).second;
        if (b->is_binary == true)
        {
            printf("Adiabatic b %d %g %g\n",b->index,b->P_orb_adiabatic_mass_loss , mass_loss_timescale);
            
            if (b->P_orb_adiabatic_mass_loss < mass_loss_timescale) // criterion for adiabatic mass loss
            {
                
                double m1_old = b->child1_mass_adiabatic_mass_loss;
                double m2_old = b->child2_mass_adiabatic_mass_loss;
                double delta_m1 = b->delta_child1_mass_adiabatic_mass_loss;
                double delta_m2 = b->delta_child2_mass_adiabatic_mass_loss;
                //double a_old = compute_a_from_h(m1_old,m2_old, norm3(b->h_vec_old_adiabatic_mass_loss), norm3(b->e_vec_old_adiabatic_mass_loss));
                //double delta_a = - (b->delta_m_adiabatic_mass_loss/(m1_old + m2_old)) * a_old;
                //double a = a_old + delta_a;
                //double h_old = compute_h_from_a(m1_old,m2_old,a,e);
                double h_old = norm3(b->h_vec_old_adiabatic_mass_loss);
                //double Delta_h = h_old * ( (delta_m1/m1_old) + (delta_m2/m2_old) - (delta_m1 + delta_m2)/(m1_old + m2_old) );
                //double h_new = h_old + Delta_h;
                double m1_new = m1_old + delta_m1;
                double m2_new = m2_old + delta_m2;
                double h_new = h_old * ((m1_old + m2_old)/(m1_new + m2_new)) * (m1_new/m1_old) * (m2_new/m2_old);
                
                //printf("Adiabatic %g %g h_old %g h_new %g delta m1 %g delta m2 %g m1_old %g m2_old %g Delta_h/h_old %g\n",b->P_orb_adiabatic_mass_loss , mass_loss_timescale,h_old,h_new,delta_m1,delta_m2,m1_old,m2_old,Delta_h/h_old);
                
                for (i=0; i<3; i++)
                {
                    /* Directions of h and e and magnitude of e do not change for adiabatic mass loss. */
                    b->h_vec[i] = (b->h_vec_old_adiabatic_mass_loss[i] / h_old) * h_new;
                    b->e_vec[i] = b->e_vec_old_adiabatic_mass_loss[i];
                }
            }
        }
    }
}



int common_envelope_evolution2(ParticlesMap *particlesMap, int binary_index, int donor_index, int accretor_index)
{
    /* Old function; can be ignored */
    Particle *binary = (*particlesMap)[binary_index];
    Particle *donor = (*particlesMap)[donor_index];
    Particle *accretor = (*particlesMap)[accretor_index];
    
    double M_donor = donor->mass;
    double M_core_donor = donor->core_mass;
    double M_envelope_donor = M_donor - M_core_donor; // do not take convective envelope mass
    double M_accretor = accretor->mass;
    
    double alpha = binary->common_envelope_alpha;
    double lambda = donor->common_envelope_lambda;
    
    double a_i = binary->a;
    double e = binary->e;
    double rp_i = a_i * (1.0 - e);
    
    double q = M_donor / M_accretor;
    double r_L_d = roche_radius_pericenter_eggleton(rp_i,q) / a_i; // take R_L at periapsis
    
    double a_f = a_i * (M_core_donor / M_donor) * M_accretor / ( M_accretor + 2.0 * M_envelope_donor / (alpha * lambda * r_L_d) );

    binary->a = a_f;
    binary->e = epsilon; // assume circularisation after CE
    donor->mass = M_core_donor; // what about stellar type?
    
    /* calc new e & h vectors; update R&V */
    return 0; 
}

int handle_wind_accretion(ParticlesMap *particlesMap, double t_old, double t, double *dt_binary_evolution, int *integration_flag)
{
    /* TO DO: since wind accretion depends on the orbit, move to ODE integration? */
    
    set_up_derived_quantities(particlesMap); /* for setting a, e, etc. */
    double v_orb_p2,v_wind_p2,factor;
    
    ParticlesMapIterator it_p;
    for (it_p = particlesMap->begin(); it_p != particlesMap->end(); it_p++)
    {

        Particle *p = (*it_p).second;
        //printf("i %d %d %d\n",donor->index,donor->is_binary,donor->is_bound);
        if (p->is_binary == false and p->is_bound == true)
        {
            /* Wind accretion from particle p onto its companion */
            Particle *parent = (*particlesMap)[p->parent];
            Particle *companion = (*particlesMap)[p->sibling];
            
            v_orb_p2 = CONST_G * parent->mass / parent->a;
            v_wind_p2 = 2.0 * beta_wind_accretion * CONST_G * p->mass / p->radius; /* squared wind speed from p */
            
            factor = (1.0/parent->j) * pow( CONST_G * companion->mass / v_wind_p2, 2.0) * (alpha_wind_accretion / (2.0 * parent->a*parent->a)) * pow(1.0 + v_orb_p2/v_wind_p2, -1.5);
            companion->mass_dot_wind_accretion = min(1.0, factor) * (- p->mass_dot_wind); /* sanity check (necessary for eccentric orbits): ensure that the companion cannot accrete more than the wind loss from p */
        }
    }
}

int stable_mass_transfer_evolution(ParticlesMap *particlesMap, int parent_index, int donor_index, int accretor_index, double t_old, double t, int *integration_flag)
{
    /* Stable mass transfer case (BSE evolv2.f: lines 1370 - 1905) */
    

   //                 donor->mass_dot_RLOF = m_dot;
//                    accretor->mass_dot_RLOF = -m_dot;

    Particle *parent = (*particlesMap)[parent_index];
    Particle *donor = (*particlesMap)[donor_index];
    Particle *accretor = (*particlesMap)[accretor_index];

    /* Reset mass_dot_RLOF; they will be calculated in this function. */
    donor->mass_dot_RLOF = 0.0;
    accretor->mass_dot_RLOF = 0.0;
   
    double P_orb = compute_orbital_period(parent);
    double m_donor = donor->mass;
    double m_accretor = accretor->mass;

    double R_donor = donor->radius;
    double R_accretor = accretor->radius;

    double m_old = m_donor + m_accretor;

    double fm = donor->fm;
    double m_dot = compute_orbit_averaged_mass_transfer_rate_emt_model(m_donor,fm,P_orb); /* NOTE: ignoring mass transfer rates as calculated in BSE */

    double dt = t - t_old;
    double dm1 = -m_dot * dt; /* amount of mass lost by donor during dt; NOTE: defined here dm1>0 */
    double dm2; /* amount of mass gained by accretor during dt (defined dm2>0) */
    
    double dms_donor = fabs(donor->mass_dot_wind * dt); /* absolute value of net mass change due to winds */
    double dms_accretor = fabs(accretor->mass_dot_wind * dt); /* absolute value of net mass change due to winds */
     
    int kw1 = donor->stellar_type;
    int kw2 = accretor->stellar_type;

    double t_KH_donor = compute_Kelvin_Helmholtz_timescale(kw1,m_donor,donor->core_mass,R_donor,donor->luminosity);
    double t_KH_accretor = compute_Kelvin_Helmholtz_timescale(kw2,m_accretor,accretor->core_mass,R_accretor,accretor->luminosity);

    double t_dyn_donor = compute_stellar_dynamical_timescale(m_donor, R_donor);

    double tms,tn;
    double *GB,*tscls,*lums;
    GB = new double[10];
    tscls = new double[20];
    lums = new double[10];  

    /* These quantities are used below to determine aging */
    star_(&kw1,&donor->sse_initial_mass,&m_donor,&tms,&tn,tscls,lums,GB,donor->zpars);
    double tms_donor_old = tms;
    double tbgb_donor_old = tscls[0];
    
    star_(&kw2,&accretor->sse_initial_mass,&m_accretor,&tms,&tn,tscls,lums,GB,accretor->zpars);
    double tms_accretor_old = tms;
    double tbgb_accretor_old = tscls[0];

    bool nova = false;

    /* Adjust dm1 based on donor type. */
    if (kw1 >= 2 and kw1 <= 9 and kw1 != 7)
    {
        /* Limit mass transfer to the thermal rate for remaining giant-like stars
        * and to the dynamical rate for all others. */

        dm1 = min(dm1, m_donor * dt/t_KH_donor);
    }
    else /* NOTE: ignoring the merger case elseif(rad(j1).gt.10.d0*rol(j1).or.(kstar(j1).le.1.and.kstar(j2).le.1.and.q(j1).gt.qc))then */
    {
        dm1 = min(dm1, m_donor * dt/t_dyn_donor);
    }

    /* Determine dm2. */
    /* Eddington accretion rate for compact accretors. */
    double hydrogen_mass_fraction = donor->zpars[10];
    double m_dot_Eddington = compute_Eddington_accretion_rate(R_accretor, hydrogen_mass_fraction);
    double dme = m_dot_Eddington * dt;

    double taum = (m_accretor/dm1) * dt;
    
    if (kw2 <= 2 or kw2 == 4)
    {
        /* Limit according to the thermal timescale of the secondary. */

        dm2 = dm1 * min(1.0, 10.0 * taum/t_KH_accretor);
    }
    else if (kw2 >= 7 and kw2 <= 9)
    {
        /* Naked helium star secondary swells up to a core helium burning star
         * or SAGB star unless the primary is also a helium star. */

        if (kw1 >= 7)
        {
            dm2 = dm1 * min(1.0, 10.0 * taum/t_KH_accretor);
        }
        else
        {
            dm2 = dm1;
            
            double dmchk = dm2 - 1.05 * dms_accretor;
            if (dmchk > 0.0 and dm2/m_accretor > 1.0e-4)
            {
                int kst = min(6, 2*kw2 - 10);
                double mcx;
                if (kst == 4)
                {
                    /* ignoring the age adjustment -- units seem to be wrong? */
                    mcx = m_accretor;
                }
                else
                {
                    mcx = accretor->core_mass;
                }
        
                double mt2 = m_accretor + (dm2 - dms_accretor); /* NOTE: think about dms_accretor */
                double accretor_age;
                gntage_(&mcx, &mt2, &kst, accretor->zpars, &accretor->sse_initial_mass, &accretor_age);
                
                accretor->epoch = t - accretor_age*Myr_to_yr;
            }
        }
    }
    else if (kw1 <= 6 and kw2 >= 10 and kw2 <= 12)
    {
        /* White dwarf secondary. */

        if (dm1/dt < 2.71e-7) 
        {
            if (dm1/dt < 1.03e-7)
            {
                /* Accrete until a nova explosion blows away most of the accreted material. */
                nova = true;

                dm2 = nova_accretion_factor * min(dm1, dme);
            }
            else
            {
                /* Steady burning at the surface */
                dm2 = dm1;
            }
                
        }
        else
        {
            /* Make a new giant envelope. */

            dm2 = dm1;

            int kw;
            if ( (kw2 == 10 and m_accretor < 0.05) or (kw2 >= 11 and m_accretor < 0.5) ) /* Check for planets or low-mass WDs. */
            {
                kw = kw2;
            }
            else
            {
                kw = min(6, 3*kw2 - 27);

                double new_age;
                double mt2 = m_accretor + (dm2 - dms_accretor);
                gntage_(&accretor->core_mass,&mt2,&kw,accretor->zpars,&accretor->sse_initial_mass,&new_age);
                accretor->age = new_age*Myr_to_yr;
                accretor->epoch = t - accretor->age;
            }
        }
    }
    else if (kw2 >= 10)
    {
        dm2 = min(dm1, dme);
    }
    else
    {
        /* We have a giant whose envelope can absorb any transferred material. */
        dm2 = dm1;
    }
    
    if (kw2 >= 10 and kw2 <= 12)
    {
        double mt2 = m_accretor + (dm2 - dms_accretor);
        if (kw1 <= 10 and kw2 == 10 and mt2 >= 0.7)
        {
            /* HeWD can only accrete helium-rich material up to a mass of 0.7 when it is destroyed in a possible Type 1a SN. */
            double Delta_m1 = dm1 + dms_donor;
            double Delta_m2 = m_accretor;
            handle_instantaneous_and_adiabatic_mass_changes_in_orbit(particlesMap, donor, accretor, Delta_m1, Delta_m2, parent->compact_object_disruption_mass_loss_timescale, integration_flag); /* TO DO: should use different ML timescale here? */

            /* The binary becomes a body with the donor's properties. */
            update_stellar_evolution_properties(donor);
            parent->is_binary = false; 
            copy_all_body_properties(donor, parent);

            particlesMap->erase(donor_index);
            particlesMap->erase(accretor_index);
        }
        else if (kw1 <= 10 and kw2 >= 11)
        {
            /* CO and ONeWDs accrete helium-rich material until the accumulated 
            * material exceeds a mass of 0.15 when it ignites. For a COWD with 
            * mass less than 0.95 the system will be destroyed as an ELD in a 
            * possible Type 1a SN. COWDs with mass greater than 0.95 and ONeWDs 
            * will survive with all the material converted to ONe (JH 30/09/99). */
    
            if (mt2 - accretor->sse_initial_mass >= 0.15)
            {
                if (kw2 == 11)
                {
                    double Delta_m1 = dm1 + dms_donor;
                    double Delta_m2 = m_accretor;
                    handle_instantaneous_and_adiabatic_mass_changes_in_orbit(particlesMap, donor, accretor, Delta_m1, Delta_m2, parent->compact_object_disruption_mass_loss_timescale, integration_flag); /* TO DO: should use different ML timescale here? */

                    /* The binary becomes a body with the donor's properties. */
                    update_stellar_evolution_properties(donor);
                    parent->is_binary = false; 
                    copy_all_body_properties(donor, parent);

                    particlesMap->erase(donor_index);
                    particlesMap->erase(accretor_index);
                }
                else
                {
                    accretor->sse_initial_mass = mt2;
                }
            }
            else
            {
                accretor->sse_initial_mass = mt2;
            }
        }
        if (kw2 == 10 or kw2 == 11)
        {
            if (mt2 >= chandrasekhar_mass)
            {
                /* If the Chandrasekhar limit is exceeded for a white dwarf then destroy
                * the white dwarf in a supernova. If the WD is ONe then a neutron star
                * will survive the supernova and we let HRDIAG take care of this when
                * the stars are next updated. */
                
                dm1 = chandrasekhar_mass - m_accretor + dms_accretor;
                double Delta_m1 = dm1 + dms_donor;
                double Delta_m2 = m_accretor;
                handle_instantaneous_and_adiabatic_mass_changes_in_orbit(particlesMap, donor, accretor, Delta_m1, Delta_m2, parent->compact_object_disruption_mass_loss_timescale, integration_flag); /* TO DO: should use different ML timescale here? */

                /* The binary becomes a body with the donor's properties. */
                update_stellar_evolution_properties(donor);
                parent->is_binary = false; 
                copy_all_body_properties(donor, parent);

                particlesMap->erase(donor_index);
                particlesMap->erase(accretor_index);
            }
        }
    }

    /* TO DO: handle changes in spins */


    /* "New" masses used for aging. Note: the actual masses (and radii) will be updated during the ODE integration.
     * Also, dm1 > 0, dm2 > 0 */
    double m_donor_new = m_donor - dm1 + donor->mass_dot_wind * dt;
    double m_accretor_new = m_accretor + dm2 + accretor->mass_dot_wind * dt;
   
    /* For a HG star check if the initial mass can be reduced. */    
    if (kw1 == 2 and donor->sse_initial_mass <= donor->zpars[2])
    {
        double m0 = m_donor_new;
        donor->sse_initial_mass = m_donor;

        star_(&kw1,&donor->sse_initial_mass,&donor->mass,&tms,&tn,tscls,lums,GB,donor->zpars);
        if (GB[8] < donor->core_mass)
        {
            donor->sse_initial_mass = m0;
        }
    }
    if (kw2 == 2 and accretor->sse_initial_mass <= accretor->zpars[2]) /* the same but with donor and accretor swapped */
    {
        double m0 = accretor->sse_initial_mass;
        accretor->sse_initial_mass = m_accretor;

        star_(&kw2,&accretor->sse_initial_mass,&accretor->mass,&tms,&tn,tscls,lums,GB,accretor->zpars);
        if (GB[8] < accretor->core_mass)
        {
            accretor->sse_initial_mass = m0;
        }
    }

    /* Always rejuvenate the secondary and age the primary if they are on the main sequence. */
    if (kw1 <= 2 or kw1 == 7)
    {
        star_(&kw1,&donor->sse_initial_mass,&m_donor_new,&tms,&tn,tscls,lums,GB,donor->zpars);
        if (kw1 == 2)
        {
            double age = tms + (donor->age*yr_to_Myr - tms_donor_old) * (tscls[0] - tms)/(tbgb_donor_old - tms_donor_old);
            donor->age *= Myr_to_yr;
        }
        else
        {
            donor->age *= (tms/tms_donor_old);
        }
        donor->epoch = t - donor->age;
    }
    if (kw2 <= 2 or kw2 == 7)
    {
        star_(&kw2,&accretor->sse_initial_mass,&m_accretor_new,&tms,&tn,tscls,lums,GB,accretor->zpars);
        if (kw2 == 2)
        {
            double age = tms + (accretor->age*yr_to_Myr - tms_accretor_old) * (tscls[0] - tms)/(tbgb_accretor_old - tms_accretor_old);
            accretor->age *= Myr_to_yr;
        }
        else if ((m_accretor_new < 0.35 or m_accretor_new > 1.25) and (kw2 != 7))
        {
            accretor->age *= (tms/tms_accretor_old) * ((m_accretor_new - dm2)/m_accretor_new);
        }
        else
        {
            accretor->age *= (tms/tms_donor_old);
        }
        accretor->epoch = t - accretor->age;
    }
    
    donor->mass_dot_RLOF = -dm1/dt;
    accretor->mass_dot_RLOF = dm2/dt;

    /* Assume that any mass not accreted is ejected from the accretor in an isotropic wind. -- TO DO: could the effect be instantaneous?
     * This somewhat mimics non-conservative mass transfer. */
    accretor->mass_dot_wind += - (dm1 - dm2);

    return 0;
}

}
