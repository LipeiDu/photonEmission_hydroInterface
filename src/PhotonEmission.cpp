// Copyright 2016 Chun Shen
#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <string>
#include <omp.h>

#include "Hydroinfo_h5.h"
#include "ThermalPhoton.h"
#include "QGP_LO.h"
//#include "QGP2to2Total.h"
//#include "QGPAMYCollinear.h"
// #include "HadronGasRhoSpectralFunction.h"
// #include "HadronGasPipiBremsstrahlung.h"
// #include "HadronGasPiRhoOmega.h"
#include "tensor_trans.h"
#include "PhotonEmission.h"
#include "ParameterReader.h"
#include "Arsenal.h"

using namespace std;
using ARSENAL::createA2DMatrix;
using ARSENAL::createA3DMatrix;
using ARSENAL::createA4DMatrix;
using ARSENAL::createA5DMatrix;
using ARSENAL::deleteA2DMatrix;
using ARSENAL::deleteA3DMatrix;
using ARSENAL::deleteA4DMatrix;
using ARSENAL::deleteA5DMatrix;
using TENSORTRANSFORM::getTransverseflow_u_mu_low;
using TENSORTRANSFORM::lorentz_boost_matrix;

using PhysConsts::hbarC;

#define CODE_TEST 0

PhotonEmission::PhotonEmission(std::shared_ptr<ParameterReader> paraRdr_in) {
    paraRdr = paraRdr_in;
    output_path = "results/";

    hydro_flag = paraRdr->getVal("hydro_flag");
    differential_flag = paraRdr->getVal("differential_flag");
    turn_off_transverse_flow = paraRdr->getVal("turn_off_transverse_flow");
    turn_on_muB_ = static_cast<int>(paraRdr->getVal("turn_on_muB", 1));


    set_hydroGridinfo();
    print_hydroGridinfo();

    // read the photon emission rate tables
    InitializePhotonEmissionRateTables();

    lambda = createA2DMatrix(4, 4, 0.);

    dNd2pTdphidy_eq = createA4DMatrix(nm, np, nphi, nrapidity, 0.);
    dNd2pTdphidy = createA4DMatrix(nm, np, nphi, nrapidity, 0.);
    dNd2pTd2M_eq = createA2DMatrix(nm, np, 0);
    dNd2pTd2M = createA2DMatrix(nm, np, 0);

    vnpT_cos_eq = createA3DMatrix(norder, nm, np, 0.);
    vnpT_sin_eq = createA3DMatrix(norder, nm, np, 0.);
    vnpT_cos = createA3DMatrix(norder, nm, np, 0.);
    vnpT_sin = createA3DMatrix(norder, nm, np, 0.);

    dNd2Mdy_eq.resize(nm, 0);
    dNd2Mdy_tot.resize(nm, 0);
    vn_cos_eq = createA2DMatrix(norder, nm, 0.);
    vn_sin_eq = createA2DMatrix(norder, nm, 0.);
    vn_cos = createA2DMatrix(norder, nm, 0.);
    vn_sin = createA2DMatrix(norder, nm, 0.);
}


PhotonEmission::~PhotonEmission() {
    deleteA4DMatrix(dNd2pTdphidy_eq, nm, np, nphi);
    deleteA4DMatrix(dNd2pTdphidy, nm, np, nphi);

    deleteA3DMatrix(vnpT_cos_eq, norder, nm);
    deleteA3DMatrix(vnpT_sin_eq, norder, nm);
    deleteA3DMatrix(vnpT_cos, norder, nm);
    deleteA3DMatrix(vnpT_sin, norder, nm);

    deleteA2DMatrix(vn_cos_eq, norder);
    deleteA2DMatrix(vn_sin_eq, norder);
    deleteA2DMatrix(vn_cos, norder);
    deleteA2DMatrix(vn_sin, norder);

    deleteA2DMatrix(lambda, 4);
}


void PhotonEmission::set_hydroGridinfo() {
    gridX0 = paraRdr->getVal("Xmin");
    gridY0 = paraRdr->getVal("Ymin");
    gridDx = paraRdr->getVal("dx");
    gridDy = paraRdr->getVal("dy");
    gridTau0 = paraRdr->getVal("tau_start");
    gridTauf = paraRdr->getVal("tau_end");
    gridDtau = paraRdr->getVal("dTau");

    gridNx = 2*fabs(gridX0)/gridDx + 1;
    gridNy = 2*fabs(gridY0)/gridDy + 1;

    nm = paraRdr->getVal("nm");
    neta = paraRdr->getVal("neta");
    np = paraRdr->getVal("np");
    nphi = paraRdr->getVal("nphi");
    nrapidity = paraRdr->getVal("nrapidity");
    norder = paraRdr->getVal("norder");

    T_dec = paraRdr->getVal("T_dec");
    T_sw_high = paraRdr->getVal("T_sw_high");
    T_sw_low = paraRdr->getVal("T_sw_low");
    T_cuthigh = paraRdr->getVal("T_cuthigh");
    T_cutlow = paraRdr->getVal("T_cutlow");
    calHGIdFlag = paraRdr->getVal("CalHGIdFlag");
}


void PhotonEmission::print_hydroGridinfo() {
    cout << "----------------------------------------" << endl;
    cout << "-- Parameters list for photon emission:" << endl;
    cout << "----------------------------------------" << endl;
    cout << "tau_start =" << paraRdr->getVal("tau_start") << " fm/c." << endl;
    cout << "tau_end =" << paraRdr->getVal("tau_end") << " fm/c." << endl;
    cout << "dTau = " << gridDtau << " fm/c" << endl;
    cout << "X_min = " << gridX0 << " fm/c" << endl;
    cout << "dx = " << gridDx << " fm/c" << endl;
    cout << "Y_min = " << gridY0 << " fm/c" << endl;
    cout << "dy = " << gridDy << " fm/c" << endl;
    cout << endl;

    cout << "T_dec = " << T_dec << " GeV." << endl;
    cout << "T_sw = " << T_sw_low <<  " to " << T_sw_high << " GeV."<< endl;
    cout << endl;

    cout << "Photon momentum: " << paraRdr->getVal("photon_q_i")
         << " to " << paraRdr->getVal("photon_q_f") << " GeV, "
         << "n_q =" << np << endl;
    cout << "Photon momentum angles: " << paraRdr->getVal("photon_phi_q_i")
         << " to " << paraRdr->getVal("photon_phi_q_f")
         << ", n_phi=" << nphi << endl;
    cout << "Photon momentum rapidity: " << paraRdr->getVal("photon_y_i")
         << " to " << paraRdr->getVal("photon_y_f")
         << ", n_y =" << nrapidity << endl;
    cout << "Dilepton invariant mass: " << paraRdr->getVal("dilepton_mass_i")
         << " to " << paraRdr->getVal("dilepton_mass_f") << " GeV, "
         << "n_m =" << nm << endl;
    cout << "Calculate individual channels in Hadron Resonance Gas phase: ";
    if (calHGIdFlag == 0) {
       cout << " No! " << endl;
    } else {
       cout << " Yes!" << endl;
    }

    
}


void PhotonEmission::InitializePhotonEmissionRateTables() {
    double photonrate_tb_Emin = paraRdr->getVal("PhotonemRatetableInfo_Emin");
    double photonrate_tb_Tmin = paraRdr->getVal("PhotonemRatetableInfo_Tmin");
    double photonrate_tb_dE = paraRdr->getVal("PhotonemRatetableInfo_dE");
    double photonrate_tb_dT = paraRdr->getVal("PhotonemRatetableInfo_dT");

    dilepton_QGP_LO = std::unique_ptr<ThermalPhoton>(
            new QGP_LO(paraRdr, "QGP_LO_total"));
    // dilepton_QGP_LO->setupEmissionrateFromFile(
    //         photonrate_tb_Tmin, photonrate_tb_dT,
    //         photonrate_tb_Emin, photonrate_tb_dE, true, true);// true for vis corrections

    photon_QGP_2_to_2 = std::unique_ptr<ThermalPhoton>(
            new QGP_LO(paraRdr, "QGP_2to2_total"));
    // photon_QGP_2_to_2->setupEmissionrateFromFile(
    //         photonrate_tb_Tmin, photonrate_tb_dT,
    //         photonrate_tb_Emin, photonrate_tb_dE, true, true);// true for vis corrections
    // photon_QGP_collinear = std::unique_ptr<ThermalPhoton>(
    //         new QGPAMYCollinear(paraRdr, "QGP_AMYcollinear"));
    // if (paraRdr->getVal("enable_polyakov_suppression") == 1) {
    //     photon_QGP_2_to_2->update_rates_with_polyakov_suppression();
    //     photon_QGP_collinear->update_rates_with_polyakov_suppression();
    // }
    // photon_HG_meson = std::unique_ptr<ThermalPhoton>(
    //         new ThermalPhoton(paraRdr, "HG_2to2_meson_total"));
    // photon_HG_meson->setupEmissionrateFromFile(
    //     photonrate_tb_Tmin, photonrate_tb_dT,
    //     photonrate_tb_Emin, photonrate_tb_dE, true, true);
    // photon_HG_rho_spectralfun = std::unique_ptr<ThermalPhoton>(
    //         new HadronGasRhoSpectralFunction(paraRdr, "HG_rho_spectralfun"));
    // photon_HG_pipiBremsstrahlung = std::unique_ptr<ThermalPhoton>(
    //         new HadronGasPipiBremsstrahlung(paraRdr, "HG_pipi_bremsstrahlung"));
    // photon_HG_omega = std::unique_ptr<ThermalPhoton>(
    //         new HadronGasPiRhoOmega(paraRdr, "HG_omega"));
    // photon_HG_omega->setupEmissionrateFromParametrization(
    //     photonrate_tb_Tmin, photonrate_tb_dT, 76,
    //     photonrate_tb_Emin, photonrate_tb_dE, 81);


    // if (calHGIdFlag == 1) {
    //     photon_pirho = std::unique_ptr<ThermalPhoton>(
    //             new ThermalPhoton(paraRdr, "pion_rho_to_pion_gamma"));
    //     photon_pirho->setupEmissionrateFromFile(
    //         photonrate_tb_Tmin, photonrate_tb_dT,
    //         photonrate_tb_Emin, photonrate_tb_dE, true, true);
    //     photon_KstarK = std::unique_ptr<ThermalPhoton>(
    //             new ThermalPhoton(paraRdr, "K_Kstar_to_pion_gamma"));
    //     photon_KstarK->setupEmissionrateFromFile(
    //         photonrate_tb_Tmin, photonrate_tb_dT,
    //         photonrate_tb_Emin, photonrate_tb_dE, true, true);
    //     photon_piK = std::unique_ptr<ThermalPhoton>(
    //             new ThermalPhoton(paraRdr, "pion_Kstar_to_K_gamma"));
    //     photon_piK->setupEmissionrateFromFile(
    //         photonrate_tb_Tmin, photonrate_tb_dT,
    //         photonrate_tb_Emin, photonrate_tb_dE, true, true);
    //     photon_piKstar = std::unique_ptr<ThermalPhoton>(
    //             new ThermalPhoton(paraRdr, "pion_Kstar_to_K_gamma"));
    //     photon_piKstar->setupEmissionrateFromFile(
    //         photonrate_tb_Tmin, photonrate_tb_dT,
    //         photonrate_tb_Emin, photonrate_tb_dE, true, true);
    //     photon_pipi = std::unique_ptr<ThermalPhoton>(
    //             new ThermalPhoton(paraRdr, "pion_pion_to_rho_gamma"));
    //     photon_pipi->setupEmissionrateFromFile(
    //         photonrate_tb_Tmin, photonrate_tb_dT,
    //         photonrate_tb_Emin, photonrate_tb_dE, true, true);
    //     photon_rhoK = std::unique_ptr<ThermalPhoton>(
    //             new ThermalPhoton(paraRdr, "rho_K_to_K_gamma"));
    //     photon_rhoK->setupEmissionrateFromFile(
    //         photonrate_tb_Tmin, photonrate_tb_dT,
    //         photonrate_tb_Emin, photonrate_tb_dE, true, true);
    //     photon_rho = std::unique_ptr<ThermalPhoton>(
    //             new ThermalPhoton(paraRdr, "rho_to_pion_pion_gamma"));
    //     photon_rho->setupEmissionrateFromFile(
    //         photonrate_tb_Tmin, photonrate_tb_dT,
    //         photonrate_tb_Emin, photonrate_tb_dE, true, true);
    //     photon_pirho_omegat = std::unique_ptr<ThermalPhoton>(
    //             new ThermalPhoton(paraRdr, "pion_rho_to_omega_to_pion_gamma"));
    //     photon_pirho_omegat->setupEmissionrateFromFile(
    //         photonrate_tb_Tmin, photonrate_tb_dT,
    //         photonrate_tb_Emin, photonrate_tb_dE, true, true);
    // }
}

// void PhotonEmission::calPhotonemission(
//         void *hydroinfo_ptr_in, double *eta_ptr, double *etaweight_ptr) {
//     HydroinfoH5 *hydroinfo_h5_ptr = nullptr;
//     Hydroinfo_MUSIC *hydroinfo_MUSIC_ptr = nullptr;
//     if (hydro_flag == 0) {
//         hydroinfo_h5_ptr = reinterpret_cast<HydroinfoH5*>(hydroinfo_ptr_in);
//     } else if (hydro_flag > 0 && hydro_flag < 4) {
//         hydroinfo_MUSIC_ptr =
//                       reinterpret_cast<Hydroinfo_MUSIC*>(hydroinfo_ptr_in);
//     }

//     // photon momentum in the lab frame
//     double p_q[np], phi_q[nphi], y_q[nrapidity];
//     double sin_phiq[nphi], cos_phiq[nphi];
//     double p_lab_local[4], p_lab_lowmu[4];
//     double flow_u_mu_low[4];
//     for (int k = 0; k < nrapidity; k++) {
//         y_q[k] = photon_QGP_2_to_2->getPhotonrapidity(k);
//     }
//     for (int l = 0; l < np; l++) {
//         p_q[l] = photon_QGP_2_to_2->getPhotonp(l);
//     }
//     for (int m = 0; m < nphi; m++) {
//         phi_q[m] = photon_QGP_2_to_2->getPhotonphi(m);
//         sin_phiq[m] = sin(phi_q[m]);
//         cos_phiq[m] = cos(phi_q[m]);
//     }

//     double e_local, p_local, temp_local, vx_local, vy_local;
//     double vz_local = 0.;
//     double bulkPi_local = 0.;
//     double tau_local = 1.;
//     double eta_local = 0.;
//     std::vector<double> volume(neta, 0);
//     double** pi_tensor_lab = createA2DMatrix(4, 4, 0.);

//     fluidCell *fluidCellptr = new fluidCell;

//     const int Eqtb_length = neta*nrapidity*np*nphi;
//     Eq_localrest_Tb.resize(Eqtb_length, 0);
//     pi_photon_Tb.resize(Eqtb_length, 0);
//     bulkPi_Tb.resize(Eqtb_length, 0);

//     // main loop begins ...
//     // loop over time frame
//     double hydro_tau_max = 0.0;
//     if (hydro_flag == 0) {
//         hydro_tau_max = hydroinfo_h5_ptr->getHydrogridTaumax();
//     } else if (hydro_flag > 0 && hydro_flag < 4) {
//         hydro_tau_max = hydroinfo_MUSIC_ptr->get_hydro_tau_max();
//     }
//     if (gridTauf > hydro_tau_max) {
//         gridTauf = hydro_tau_max;
//     }
//     int nFrame = static_cast<int>((gridTauf - gridTau0)/gridDtau + 1e-15) + 1;
//     for (int frameId = 0; frameId < nFrame; frameId++) {
//         tau_local = gridTau0 + frameId*gridDtau;

//         // volume element: tau*dtau*dx*dy*deta,
//         // 2 for symmetry along longitudinal direction
//         for (int k = 0; k < neta; k++) {
//             volume[k] = 2*tau_local*gridDx*gridDy*gridDtau*etaweight_ptr[k];
//         }

//         // loops over the transverse plane
//         for (int i = 0; i < gridNx; i++) {
//             double x_local = gridX0 + i*gridDx;
//             for (int j = 0; j < gridNy; j++) {
//                 double y_local = gridY0 + j*gridDy;

//                 int idx_Tb = 0;
//                 if (hydro_flag == 0) {
//                     hydroinfo_h5_ptr->getHydroinfo(
//                         tau_local, x_local, y_local, fluidCellptr);
//                 } else if (hydro_flag > 0 && hydro_flag < 4) {
//                     // for boost-invariant calculations
//                     // only get medium at eta = 0
//                     hydroinfo_MUSIC_ptr->getHydroValues(
//                         x_local, y_local, 0.0, tau_local, fluidCellptr);
//                 }
//                 temp_local = fluidCellptr->temperature;

//                 if (temp_local < T_dec || temp_local > T_cuthigh
//                     || temp_local < T_cutlow) {
//                     // fluid cell is out of interest
//                     continue;
//                 }

//                 e_local = fluidCellptr->ed;
//                 p_local = fluidCellptr->pressure;

//                 if (turn_off_transverse_flow == 1) {
//                     vx_local = 0.0;
//                     vy_local = 0.0;
//                 } else {
//                     vx_local = fluidCellptr->vx;
//                     vy_local = fluidCellptr->vy;
//                     vz_local = fluidCellptr->vz;
//                 }

//                 for (int mu = 0; mu < 4; mu++) {
//                     for (int nu = 0; nu < 4; nu++) {
//                         pi_tensor_lab[mu][nu] = fluidCellptr->pi[mu][nu];
//                     }
//                 }
//                 bulkPi_local = fluidCellptr->bulkPi;

//                 getTransverseflow_u_mu_low(flow_u_mu_low,
//                                            vx_local, vy_local, vz_local);
//                 double prefactor_pimunu = 1./(2.*(e_local + p_local));
//                 for (int jj = 0; jj < neta; jj++) {
//                     eta_local = eta_ptr[jj];

//                     // photon momentum loops
//                     for (int k = 0; k < nrapidity; k++) {
//                         double cosh_y_minus_eta = cosh(y_q[k] - eta_local);
//                         double sinh_y_minus_eta = sinh(y_q[k] - eta_local);
//                         for (int m = 0; m < nphi; m++) {
//                             for (int l = 0; l < np; l++) {
//                                 p_lab_local[0] = p_q[l]*cosh_y_minus_eta;
//                                 p_lab_local[1] = p_q[l]*cos_phiq[m];
//                                 p_lab_local[2] = p_q[l]*sin_phiq[m];
//                                 p_lab_local[3] = p_q[l]*sinh_y_minus_eta;
//                                 p_lab_lowmu[0] = p_lab_local[0];

//                                 for (int local_i = 1; local_i < 4; local_i++)
//                                     p_lab_lowmu[local_i] =
//                                                 - p_lab_local[local_i];

//                                 double Eq_localrest_temp = 0.0e0;
//                                 double pi_photon = 0.0e0;
//                                 for (int local_i = 0; local_i < 4; local_i++)
//                                     Eq_localrest_temp += (
//                                         flow_u_mu_low[local_i]
//                                         *p_lab_local[local_i]);

//                                 for (int local_i = 0; local_i < 4; local_i++) {
//                                     for (int local_j = 0; local_j < 4;
//                                          local_j++) {
//                                         pi_photon += (
//                                             pi_tensor_lab[local_i][local_j]
//                                                 *p_lab_lowmu[local_i]
//                                                 *p_lab_lowmu[local_j]);
//                                     }
//                                 }

//                                 Eq_localrest_Tb[idx_Tb] = Eq_localrest_temp;
//                                 pi_photon_Tb[idx_Tb] =
//                                                     pi_photon*prefactor_pimunu;
//                                 bulkPi_Tb[idx_Tb] = bulkPi_local;
//                                 idx_Tb++;
//                             }
//                         }
//                     }
//                 }

//                 // begin to calculate thermal photon emission
//                 if (temp_local > T_sw_high) {   // QGP emission
//                     double QGP_fraction = 1.0;
//                     photon_QGP_2_to_2->calThermalPhotonemission(
//                         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                         temp_local, volume, QGP_fraction);
//                     photon_QGP_collinear->calThermalPhotonemission(
//                         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                         temp_local, volume, QGP_fraction);
//                     if (differential_flag == 1 || differential_flag > 10) {
//                         photon_QGP_2_to_2->calThermalPhotonemissiondTdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, tau_local, volume, QGP_fraction);
//                         photon_QGP_collinear->calThermalPhotonemissiondTdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, tau_local, volume, QGP_fraction);
//                     }
//                     if (differential_flag == 2 || differential_flag > 10) {
//                         photon_QGP_2_to_2->calThermalPhotonemissiondxperpdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, x_local, tau_local, volume,
//                             QGP_fraction);
//                         photon_QGP_collinear->
//                             calThermalPhotonemissiondxperpdtau(
//                                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
//                                 idx_Tb, temp_local, x_local, tau_local, volume,
//                                 QGP_fraction);
//                     }
//                 } else if (temp_local > T_sw_low) {     // QGP and HG emission
//                     double QGP_fraction = (
//                                 (temp_local - T_sw_low)/(T_sw_high - T_sw_low));
//                     double HG_fraction = 1. - QGP_fraction;
//                     photon_QGP_2_to_2->calThermalPhotonemission(
//                         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                         temp_local, volume, QGP_fraction);
//                     photon_QGP_collinear->calThermalPhotonemission(
//                         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                         temp_local, volume, QGP_fraction);
//                     photon_HG_meson->calThermalPhotonemission(
//                         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                         temp_local, volume, HG_fraction);
//                     photon_HG_omega->calThermalPhotonemission(
//                         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                         temp_local, volume, HG_fraction);
//                     photon_HG_rho_spectralfun->calThermalPhotonemission(
//                         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                         temp_local, volume, HG_fraction);
//                     photon_HG_pipiBremsstrahlung->calThermalPhotonemission(
//                         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                         temp_local, volume, HG_fraction);
//                     if (differential_flag == 1 || differential_flag > 10) {
//                         photon_QGP_2_to_2->calThermalPhotonemissiondTdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, tau_local, volume, QGP_fraction);
//                         photon_QGP_collinear->calThermalPhotonemissiondTdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, tau_local, volume, QGP_fraction);
//                         photon_HG_meson->calThermalPhotonemissiondTdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, tau_local, volume, HG_fraction);
//                         photon_HG_omega->calThermalPhotonemissiondTdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, tau_local, volume, HG_fraction);
//                         photon_HG_rho_spectralfun->
//                             calThermalPhotonemissiondTdtau(
//                                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
//                                 idx_Tb, temp_local, tau_local, volume,
//                                 HG_fraction);
//                         photon_HG_pipiBremsstrahlung->
//                             calThermalPhotonemissiondTdtau(
//                                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
//                                 idx_Tb, temp_local, tau_local, volume,
//                                 HG_fraction);
//                     }
//                     if (differential_flag == 2 || differential_flag > 10) {
//                         photon_QGP_2_to_2->calThermalPhotonemissiondxperpdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, x_local, tau_local, volume,
//                             QGP_fraction);
//                         photon_QGP_collinear->
//                             calThermalPhotonemissiondxperpdtau(
//                                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
//                                 idx_Tb, temp_local, x_local, tau_local, volume,
//                                 QGP_fraction);
//                         photon_HG_meson->calThermalPhotonemissiondxperpdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, x_local, tau_local, volume,
//                             HG_fraction);
//                         photon_HG_omega->calThermalPhotonemissiondxperpdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, x_local, tau_local, volume,
//                             HG_fraction);
//                         photon_HG_rho_spectralfun->
//                             calThermalPhotonemissiondxperpdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, x_local, tau_local, volume,
//                             HG_fraction);
//                         photon_HG_pipiBremsstrahlung->
//                             calThermalPhotonemissiondxperpdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, x_local, tau_local, volume,
//                             HG_fraction);
//                     }
//                     if (calHGIdFlag == 1) {
//                         photon_pirho->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_KstarK->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_piK->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_piKstar->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_pipi->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_rhoK->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_rho->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_pirho_omegat->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                     }
//                 } else {        // hadronic emission
//                     double HG_fraction = 1.0;
//                     photon_HG_meson->calThermalPhotonemission(
//                         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                         temp_local, volume, HG_fraction);
//                     photon_HG_omega->calThermalPhotonemission(
//                         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                         temp_local, volume, HG_fraction);
//                     photon_HG_rho_spectralfun->calThermalPhotonemission(
//                         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                         temp_local, volume, HG_fraction);
//                     photon_HG_pipiBremsstrahlung->calThermalPhotonemission(
//                         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                         temp_local, volume, HG_fraction);
//                     if (differential_flag == 1 || differential_flag > 10) {
//                         photon_HG_meson->calThermalPhotonemissiondTdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, tau_local, volume, HG_fraction);
//                         photon_HG_omega->calThermalPhotonemissiondTdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, tau_local, volume, HG_fraction);
//                         photon_HG_rho_spectralfun->
//                             calThermalPhotonemissiondTdtau(
//                                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
//                                 idx_Tb, temp_local, tau_local, volume,
//                                 HG_fraction);
//                         photon_HG_pipiBremsstrahlung->
//                             calThermalPhotonemissiondTdtau(
//                                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
//                                 idx_Tb, temp_local, tau_local, volume,
//                                 HG_fraction);
//                     }
//                     if (differential_flag == 2 || differential_flag > 10) {
//                         photon_HG_meson->calThermalPhotonemissiondxperpdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, x_local, tau_local, volume,
//                             HG_fraction);
//                         photon_HG_omega->calThermalPhotonemissiondxperpdtau(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local, x_local, tau_local, volume,
//                             HG_fraction);
//                         photon_HG_rho_spectralfun->
//                             calThermalPhotonemissiondxperpdtau(
//                                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
//                                 idx_Tb, temp_local, x_local, tau_local, volume,
//                                 HG_fraction);
//                         photon_HG_pipiBremsstrahlung->
//                             calThermalPhotonemissiondxperpdtau(
//                                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
//                                 idx_Tb, temp_local, x_local, tau_local, volume,
//                                 HG_fraction);
//                     }
//                     if (calHGIdFlag == 1) {
//                         photon_pirho->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_KstarK->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_piK->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_piKstar->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_pipi->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_rhoK->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_rho->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                         photon_pirho_omegat->calThermalPhotonemission(
//                             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb, idx_Tb,
//                             temp_local,  volume, HG_fraction);
//                     }
//                 }
//             }
//         }
//         cout << "frame " << frameId << " : ";
//         cout << " tau = " << setw(4) << setprecision(3) << tau_local
//              << " fm/c done!" << endl;
//     }

//     delete fluidCellptr;
//     deleteA2DMatrix(pi_tensor_lab, 4);
// }


void PhotonEmission::calPhotonemission_3d(void *hydroinfo_ptr_in) {
    Hydroinfo_MUSIC *hydroinfo_MUSIC_ptr;
    hydroinfo_MUSIC_ptr =
                reinterpret_cast<Hydroinfo_MUSIC*>(hydroinfo_ptr_in);

    // photon momentum in the lab frame
    double M_ll[nm]; // invariant mass array
    double p_q[np], phi_q[nphi], y_q[nrapidity];
    double sin_phiq[nphi], cos_phiq[nphi];
    double p_lab_local[4];
    double p_lab_Min[4]; // dilepton 4-momentum in Minkowski coordinates
    for (int k = 0; k < nrapidity; k++) {
        y_q[k] = photon_QGP_2_to_2->getPhotonrapidity(k);
    }
    for (int l = 0; l < np; l++) {
        p_q[l] = photon_QGP_2_to_2->getPhotonp(l);
    }
    for (int m = 0; m < nphi; m++) {
        phi_q[m] = photon_QGP_2_to_2->getPhotonphi(m);
        sin_phiq[m] = sin(phi_q[m]);
        cos_phiq[m] = cos(phi_q[m]);
    }
    for (int j = 0; j < nm; j++) {
        M_ll[j] = photon_QGP_2_to_2->getDileptonMass(j);
    }

    // get hydro grid information
    double tau0 = hydroinfo_MUSIC_ptr->get_hydro_tau0();
    double dtau = hydroinfo_MUSIC_ptr->get_hydro_dtau();
    double dx = hydroinfo_MUSIC_ptr->get_hydro_dx();
    double deta = hydroinfo_MUSIC_ptr->get_hydro_deta();
    double eta_max = hydroinfo_MUSIC_ptr->get_hydro_eta_max();
    double X_max = hydroinfo_MUSIC_ptr->get_hydro_x_max();
    double Nskip_x = hydroinfo_MUSIC_ptr->get_hydro_Nskip_x();
    double Nskip_eta = hydroinfo_MUSIC_ptr->get_hydro_Nskip_eta();
    double Nskip_tau = hydroinfo_MUSIC_ptr->get_hydro_Nskip_tau();
    double volume_base = Nskip_tau*dtau*Nskip_x*dx*Nskip_x*dx*Nskip_eta*deta;

    const int Eqtb_length = nrapidity*np*nphi*nm;
    Eq_localrest_Tb.resize(Eqtb_length, 0);
    pi_photon_Tb.resize(Eqtb_length, 0);
    bulkPi_Tb.resize(Eqtb_length, 0);
    diff_Tb.resize(Eqtb_length, 0);

    double tau_now = 0.0;
    double flow_u_mu_low[4];
    double flow_u_mu_Min[4]; // fluid velocity in Minkowski
    fluidCell_3D_new fluidCellptr;
    double **lambda_munu = createA2DMatrix(4, 4, 0.);

    // main loop begins ...
    // loop over all fluid cells
    long int number_of_cells =(
                hydroinfo_MUSIC_ptr->get_number_of_fluid_cells_3d());
    cout << "number of cells:" << number_of_cells << endl;

    cout << "----------------------------------------" << endl;

    int ncells = 0;

    for (long int cell_id = 0; cell_id < number_of_cells; cell_id++) {

// #ifdef _OPENMP
//         int thread_id = omp_get_thread_num();
//         int num_threads = omp_get_num_threads();

//         std::cout << "Hello from thread " << thread_id << " of " << num_threads << " threads" << std::endl;
// #endif

        hydroinfo_MUSIC_ptr->get_hydro_cell_info_3d(cell_id, fluidCellptr);

        double tau_local = tau0 + fluidCellptr.itau*dtau;
        double eta_local = -eta_max + fluidCellptr.ieta*deta;
        double x_local = -X_max + fluidCellptr.ix*dx;
        if (fabs(tau_now - tau_local) > 1e-10) {
            tau_now = tau_local;
            cout << "Calculating tau = " << setw(4) << setprecision(3)
                 << tau_now << " fm/c..." << endl;
        }

        // volume element: tau*dtau*dx*dy*deta,
        double volume = tau_local*volume_base;

        const double ed_local = fluidCellptr.ed;
        const double pd_local = fluidCellptr.pressure;
        double temp_local = fluidCellptr.temperature;
        double muB_local = turn_on_muB_*fluidCellptr.muB;
        const double rhoB_local = turn_on_muB_*fluidCellptr.rhoB;
        double temp_inv = 1/temp_local;
        double rhoB_over_eplusp = rhoB_local/(ed_local+pd_local);

        if (temp_local < T_dec ||
            temp_local > T_cuthigh || temp_local < T_cutlow) {
            // fluid cell is out of interest
            continue;
        }
        double ux, uy, ueta;
        if (turn_off_transverse_flow == 1) {
            ux = 0.0;
            uy = 0.0;
            ueta = 0.0;
        } else {
            ux = fluidCellptr.ux;
            uy = fluidCellptr.uy;
            ueta = fluidCellptr.ueta;
        }


        // validation setup
        if(CODE_TEST){
            temp_local = 0.25;
            temp_inv = 1/temp_local;
            muB_local = 0.9;
            rhoB_over_eplusp = 8.0;
            volume = 1.0;
            eta_local = 0.0;
            ux = 0.0;
            uy = 0.0;
            ueta = 0.0;
        }

        double utau = sqrt(1. + ux*ux + uy*uy + ueta*ueta);

        flow_u_mu_low[0] = utau;
        flow_u_mu_low[1] = -ux;
        flow_u_mu_low[2] = -uy;
        flow_u_mu_low[3] = -ueta;

        double cosh_eta = cosh(eta_local);
        double sinh_eta = sinh(eta_local);

        // 4 velocity in Minkowski in lab frame

        flow_u_mu_Min[0] = cosh_eta*utau + sinh_eta*ueta;
        flow_u_mu_Min[1] = ux;
        flow_u_mu_Min[2] = uy;
        flow_u_mu_Min[3] = sinh_eta*utau + cosh_eta*ueta;

        // Lorentz boost matrix from lab to local rest frame

        lorentz_boost_matrix(lambda_munu, flow_u_mu_Min[0], 
            flow_u_mu_Min[1], flow_u_mu_Min[2], flow_u_mu_Min[3]);

        double pi11 = fluidCellptr.pi11;
        double pi12 = fluidCellptr.pi12;
        double pi13 = fluidCellptr.pi13;
        double pi22 = fluidCellptr.pi22;
        double pi23 = fluidCellptr.pi23;
        // reconstruct all other components of the shear stress tensor
        double pi01 = (ux*pi11 + uy*pi12 + ueta*pi13)/utau;
        double pi02 = (ux*pi12 + uy*pi22 + ueta*pi23)/utau;
        double pi33 = (utau*(ux*pi01 + uy*pi02) - utau*utau*(pi11 + pi22)
                       + ueta*(ux*pi13 + uy*pi23))/(utau*utau - ueta*ueta);
        double pi00 = pi11 + pi22 + pi33;
        double pi03 = (ux*pi13 + uy*pi23 + ueta*pi33)/utau;

        double bulkPi_local = fluidCellptr.bulkPi;

        // qeta = tau*qeta, qi is qi/kappa_hat
        double qx = fluidCellptr.qx;
        double qy = fluidCellptr.qy;
        double qeta = fluidCellptr.qz;
        double qtau = (ux*qx + uy*qy + ueta*qeta)/utau;

        double prefactor_pimunu = 1./2.;
        double prefactor_diff = 1./(4.*pow(2*M_PI, 5)) * temp_inv/pow(hbarC, 4);

        int idx_Tb = 0;

        // photon momentum loops
        for (int k = 0; k < nrapidity; k++) {
            double cosh_y = cosh(y_q[k]);
            double sinh_y = sinh(y_q[k]);
            double cosh_y_minus_eta = cosh(y_q[k] - eta_local);
            double sinh_y_minus_eta = sinh(y_q[k] - eta_local);
            //#pragma omp parallel for collapse(3)
            for (int m = 0; m < nphi; m++) {
                for (int l = 0; l < np; l++) {
                    for (int j = 0; j < nm; j++) {

                        double M_T = sqrt(p_q[l]*p_q[l]+M_ll[j]*M_ll[j]); // transverse mass

                        // p_q is p_T array
                        // Minkowski four momentum in lab frame
                        p_lab_Min[0] = M_T * cosh_y;
                        p_lab_Min[1] = p_q[l]*cos_phiq[m];
                        p_lab_Min[2] = p_q[l]*sin_phiq[m];
                        p_lab_Min[3] = M_T * sinh_y;

                        // from Minkowski to Milne in lab frame
                        p_lab_local[0] = M_T * cosh_y_minus_eta;
                        p_lab_local[1] = p_q[l]*cos_phiq[m];
                        p_lab_local[2] = p_q[l]*sin_phiq[m];
                        p_lab_local[3] = M_T * sinh_y_minus_eta;

                        // from Lab frame to LRF frame
                        double p_Min_lrf[4];
                        for (int j = 0; j < 4; j++) {
                            p_Min_lrf[j] = 0.;
                            for (int i = 0; i < 4; i++) {
                                p_Min_lrf[j] += lambda_munu[j][i]*p_lab_Min[i];
                            }                            
                        }

                        double pvec_lrf, pvec3; // spatial magnitude, |vec p| and |vec p|^5
                        pvec_lrf = sqrt(p_Min_lrf[1]*p_Min_lrf[1] + p_Min_lrf[2]*p_Min_lrf[2] + p_Min_lrf[3]*p_Min_lrf[3]);
                        pvec3 = pow(pvec_lrf, 3);

                        double Eq_localrest_temp = 0.0e0;
                        for (int local_i = 0; local_i < 4; local_i++) {
                            Eq_localrest_temp += (flow_u_mu_low[local_i]
                                                  *p_lab_local[local_i]);
                        }

                        double pi_photon = (
                              p_lab_local[0]*p_lab_local[0]*pi00
                            - 2.*p_lab_local[0]*p_lab_local[1]*pi01
                            - 2.*p_lab_local[0]*p_lab_local[2]*pi02
                            - 2.*p_lab_local[0]*p_lab_local[3]*pi03
                            + p_lab_local[1]*p_lab_local[1]*pi11
                            + 2.*p_lab_local[1]*p_lab_local[2]*pi12
                            + 2.*p_lab_local[1]*p_lab_local[3]*pi13
                            + p_lab_local[2]*p_lab_local[2]*pi22
                            + 2.*p_lab_local[2]*p_lab_local[3]*pi23
                            + p_lab_local[3]*p_lab_local[3]*pi33);

                        // dot product of diffusion and dilepton momentum, calculated in lab frame
                        // note that 1/kappa_hat included
                        double diff_dot_p = qtau*p_lab_local[0] - qx*p_lab_local[1] 
                                            - qy*p_lab_local[2] - qeta*p_lab_local[3];
                        // validation setup
                        if(CODE_TEST){
                            diff_dot_p = 1.0;
                        }
                        
                        Eq_localrest_Tb[idx_Tb] = Eq_localrest_temp;
                        pi_photon_Tb[idx_Tb] = pi_photon*prefactor_pimunu;
                        bulkPi_Tb[idx_Tb] = bulkPi_local;
                        diff_Tb[idx_Tb] = prefactor_diff * diff_dot_p * M_ll[j] * M_ll[j] / pvec3;
                        idx_Tb++; // Tb_length = nrapidity*nphi*np

                    }
                }
            }
        }

        
        // begin to calculate thermal photon emission
        if (temp_local > T_sw_high) {   // QGP emission
            double QGP_fraction = 1.0;
            // photon_QGP_2_to_2->calThermalPhotonemission_3d(
            //     Eq_localrest_Tb, M_ll, pi_photon_Tb, bulkPi_Tb,
            //     temp_local, muB_local, volume, QGP_fraction);
            // photon_QGP_collinear->calThermalPhotonemission_3d(
            //     Eq_localrest_Tb, M_ll, pi_photon_Tb, bulkPi_Tb,
            //     temp_local, muB_local, volume, QGP_fraction);
            dilepton_QGP_LO->calThermalPhotonemission_3d(
                Eq_localrest_Tb, M_ll, pi_photon_Tb, bulkPi_Tb, diff_Tb,
                temp_local, muB_local, rhoB_over_eplusp, volume, QGP_fraction);
            ncells++;

            if (differential_flag == 1 || differential_flag > 10) {
                dilepton_QGP_LO->calThermalPhotonemissiondTdtau_3d(
                    Eq_localrest_Tb, M_ll, pi_photon_Tb, bulkPi_Tb, diff_Tb,
                    temp_local, muB_local, rhoB_over_eplusp, tau_local, volume, QGP_fraction);
                // photon_QGP_collinear->calThermalPhotonemissiondTdtau_3d(
                //     Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
                //     temp_local, muB_local, tau_local, volume, QGP_fraction);
            }
            // if (differential_flag == 2 || differential_flag > 10) {
            //     photon_QGP_2_to_2->calThermalPhotonemissiondxperpdtau_3d(
            //         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
            //         temp_local, muB_local, x_local, tau_local, volume,
            //         QGP_fraction);
            //     photon_QGP_collinear->calThermalPhotonemissiondxperpdtau_3d(
            //         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
            //         temp_local, muB_local, x_local, tau_local, volume,
            //         QGP_fraction);
            // }
        }
        
        // else if (temp_local > T_sw_low) {     // QGP and HG emission
        //     double QGP_fraction = (
        //                 (temp_local - T_sw_low)/(T_sw_high - T_sw_low));
        //     double HG_fraction = 1. - QGP_fraction;
        //     photon_QGP_2_to_2->calThermalPhotonemission_3d(
        //         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //         temp_local, muB_local, volume, QGP_fraction);
        //     photon_QGP_collinear->calThermalPhotonemission_3d(
        //         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //         temp_local, muB_local, volume, QGP_fraction);
        //     photon_HG_meson->calThermalPhotonemission_3d(
        //         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //         temp_local, muB_local, volume, HG_fraction);
        //     photon_HG_omega->calThermalPhotonemission_3d(
        //         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //         temp_local, muB_local, volume, HG_fraction);
        //     photon_HG_rho_spectralfun->calThermalPhotonemission_3d(
        //         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //         temp_local, muB_local, volume, HG_fraction);
        //     photon_HG_pipiBremsstrahlung->calThermalPhotonemission_3d(
        //         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //         temp_local, muB_local, volume, HG_fraction);
        //     if (differential_flag == 1 || differential_flag > 10) {
        //         photon_QGP_2_to_2->calThermalPhotonemissiondTdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, tau_local, volume, QGP_fraction);
        //         photon_QGP_collinear->calThermalPhotonemissiondTdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, tau_local, volume, QGP_fraction);
        //         photon_HG_meson->calThermalPhotonemissiondTdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, tau_local, volume, HG_fraction);
        //         photon_HG_omega->calThermalPhotonemissiondTdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, tau_local, volume, HG_fraction);
        //         photon_HG_rho_spectralfun->calThermalPhotonemissiondTdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, tau_local, volume,
        //             HG_fraction);
        //         photon_HG_pipiBremsstrahlung->
        //             calThermalPhotonemissiondTdtau_3d(
        //                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //                 temp_local, muB_local, tau_local, volume,
        //                 HG_fraction);
        //     }
        //     if (differential_flag == 2 || differential_flag > 10) {
        //         photon_QGP_2_to_2->calThermalPhotonemissiondxperpdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, x_local, tau_local, volume,
        //             QGP_fraction);
        //         photon_QGP_collinear->calThermalPhotonemissiondxperpdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, x_local, tau_local, volume,
        //             QGP_fraction);
        //         photon_HG_meson->calThermalPhotonemissiondxperpdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, x_local, tau_local, volume,
        //             HG_fraction);
        //         photon_HG_omega->calThermalPhotonemissiondxperpdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, x_local, tau_local, volume,
        //             HG_fraction);
        //         photon_HG_rho_spectralfun->
        //             calThermalPhotonemissiondxperpdtau_3d(
        //                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //                 temp_local, muB_local, x_local, tau_local, volume,
        //                 HG_fraction);
        //         photon_HG_pipiBremsstrahlung->
        //             calThermalPhotonemissiondxperpdtau_3d(
        //                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //                 temp_local, muB_local, x_local, tau_local, volume,
        //                 HG_fraction);
        //     }
        //     if (calHGIdFlag == 1) {
        //         photon_pirho->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_KstarK->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_piK->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_piKstar->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_pipi->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_rhoK->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_rho->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_pirho_omegat->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //     }
        // } else {        // hadronic emission
        //     double HG_fraction = 1.0;
        //     photon_HG_meson->calThermalPhotonemission_3d(
        //         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //         temp_local, muB_local, volume, HG_fraction);
        //     photon_HG_omega->calThermalPhotonemission_3d(
        //         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //         temp_local, muB_local, volume, HG_fraction);
        //     photon_HG_rho_spectralfun->calThermalPhotonemission_3d(
        //         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //         temp_local, muB_local, volume, HG_fraction);
        //     photon_HG_pipiBremsstrahlung->calThermalPhotonemission_3d(
        //         Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //         temp_local, muB_local, volume, HG_fraction);
        //     if (differential_flag == 1 || differential_flag > 10) {
        //         photon_HG_meson->calThermalPhotonemissiondTdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, tau_local, volume, HG_fraction);
        //         photon_HG_omega->calThermalPhotonemissiondTdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, tau_local, volume, HG_fraction);
        //         photon_HG_rho_spectralfun->
        //             calThermalPhotonemissiondTdtau_3d(
        //                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //                 temp_local, muB_local, tau_local, volume,
        //                 HG_fraction);
        //         photon_HG_pipiBremsstrahlung->
        //             calThermalPhotonemissiondTdtau_3d(
        //                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //                 temp_local, muB_local, tau_local, volume,
        //                 HG_fraction);
        //     }
        //     if (differential_flag == 2 || differential_flag > 10) {
        //         photon_HG_meson->calThermalPhotonemissiondxperpdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, x_local, tau_local, volume,
        //             HG_fraction);
        //         photon_HG_omega->calThermalPhotonemissiondxperpdtau_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, x_local, tau_local, volume,
        //             HG_fraction);
        //         photon_HG_rho_spectralfun->
        //             calThermalPhotonemissiondxperpdtau_3d(
        //                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //                 temp_local, muB_local, x_local, tau_local, volume,
        //                 HG_fraction);
        //         photon_HG_pipiBremsstrahlung->
        //             calThermalPhotonemissiondxperpdtau_3d(
        //                 Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //                 temp_local, muB_local, x_local, tau_local, volume,
        //                 HG_fraction);
        //     }
        //     if (calHGIdFlag == 1) {
        //         photon_pirho->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_KstarK->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_piK->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_piKstar->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_pipi->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_rhoK->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_rho->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //         photon_pirho_omegat->calThermalPhotonemission_3d(
        //             Eq_localrest_Tb, pi_photon_Tb, bulkPi_Tb,
        //             temp_local, muB_local, volume, HG_fraction);
        //     }
        // }
    }

    if(CODE_TEST){
        printf("Cells above T_sw_high=%d...\n", ncells);
    }

    //printf("error H...\n");

    deleteA2DMatrix(lambda_munu, 4);
}


void PhotonEmission::calPhoton_SpvnpT_individualchannel() {
    // dilepton_QGP_LO->calPhoton_SpvnpT_shell();
//     photon_QGP_collinear->calPhoton_SpvnpT_shell();
//     photon_HG_meson->calPhoton_SpvnpT_shell();
//     photon_HG_omega->calPhoton_SpvnpT_shell();
//     photon_HG_rho_spectralfun->calPhoton_SpvnpT_shell();
//     photon_HG_pipiBremsstrahlung->calPhoton_SpvnpT_shell();
    if (differential_flag == 1 || differential_flag > 10) {
        dilepton_QGP_LO->calPhoton_SpvnpT_dTdtau();
//         photon_QGP_collinear->calPhoton_SpvnpT_dTdtau();
//         photon_HG_meson->calPhoton_SpvnpT_dTdtau();
//         photon_HG_omega->calPhoton_SpvnpT_dTdtau();
//         photon_HG_rho_spectralfun->calPhoton_SpvnpT_dTdtau();
//         photon_HG_pipiBremsstrahlung->calPhoton_SpvnpT_dTdtau();
    }
//     if (differential_flag == 2 || differential_flag > 10) {
//         photon_QGP_2_to_2->calPhoton_SpvnpT_dxperpdtau();
//         photon_QGP_collinear->calPhoton_SpvnpT_dxperpdtau();
//         photon_HG_meson->calPhoton_SpvnpT_dxperpdtau();
//         photon_HG_omega->calPhoton_SpvnpT_dxperpdtau();
//         photon_HG_rho_spectralfun->calPhoton_SpvnpT_dxperpdtau();
//         photon_HG_pipiBremsstrahlung->calPhoton_SpvnpT_dxperpdtau();
//     }
//     if (calHGIdFlag == 1) {
//         photon_pirho->calPhoton_SpvnpT_shell();
//         photon_KstarK->calPhoton_SpvnpT_shell();
//         photon_piK->calPhoton_SpvnpT_shell();
//         photon_piKstar->calPhoton_SpvnpT_shell();
//         photon_pipi->calPhoton_SpvnpT_shell();
//         photon_rhoK->calPhoton_SpvnpT_shell();
//         photon_rho->calPhoton_SpvnpT_shell();
//         photon_pirho_omegat->calPhoton_SpvnpT_shell();
//     }
}


void PhotonEmission::outputPhotonSpvn_individualchannel() {
    // dilepton_QGP_LO->outputPhoton_SpvnpT_shell(output_path);
//     // photon_QGP_collinear->outputPhoton_SpvnpT_shell(output_path);
//     // photon_HG_meson->outputPhoton_SpvnpT_shell(output_path);
//     // photon_HG_omega->outputPhoton_SpvnpT_shell(output_path);
//     // photon_HG_rho_spectralfun->outputPhoton_SpvnpT_shell(output_path);
//     // photon_HG_pipiBremsstrahlung->outputPhoton_SpvnpT_shell(output_path);
    if (differential_flag == 1 || differential_flag > 10) {
        dilepton_QGP_LO->outputPhoton_SpvnpT_dTdtau(output_path);
        dilepton_QGP_LO->outputPhoton_spectra_dTdtau(output_path);
//     //     photon_QGP_collinear->outputPhoton_SpvnpTdTdtau(output_path);
//     //     photon_QGP_2_to_2->output_photon_spectra_dTdtau(output_path);
//     //     photon_QGP_collinear->output_photon_spectra_dTdtau(output_path);
//     //     photon_HG_meson->outputPhoton_SpvnpTdTdtau(output_path);
//     //     photon_HG_meson->output_photon_spectra_dTdtau(output_path);
//     //     photon_HG_omega->outputPhoton_SpvnpTdTdtau(output_path);
//     //     photon_HG_omega->output_photon_spectra_dTdtau(output_path);
//     //     photon_HG_rho_spectralfun->outputPhoton_SpvnpTdTdtau(output_path);
//     //     photon_HG_rho_spectralfun->output_photon_spectra_dTdtau(output_path);
//     //     photon_HG_pipiBremsstrahlung->outputPhoton_SpvnpTdTdtau(output_path);
//     //     photon_HG_pipiBremsstrahlung->output_photon_spectra_dTdtau(output_path);
    }
//     // if (differential_flag == 2 || differential_flag > 10) {
//     //     photon_QGP_2_to_2->outputPhoton_SpvnpTdxperpdtau(output_path);
//     //     photon_QGP_collinear->outputPhoton_SpvnpTdxperpdtau(output_path);
//     //     photon_HG_meson->outputPhoton_SpvnpTdxperpdtau(output_path);
//     //     photon_HG_omega->outputPhoton_SpvnpTdxperpdtau(output_path);
//     //     photon_HG_rho_spectralfun->outputPhoton_SpvnpTdxperpdtau(output_path);
//     //     photon_HG_pipiBremsstrahlung->outputPhoton_SpvnpTdxperpdtau(output_path);
//     // }
//     // if (calHGIdFlag == 1) {
//     //     photon_pirho->outputPhoton_SpvnpT_shell(output_path);
//     //     photon_KstarK->outputPhoton_SpvnpT_shell(output_path);
//     //     photon_piK->outputPhoton_SpvnpT_shell(output_path);
//     //     photon_piKstar->outputPhoton_SpvnpT_shell(output_path);
//     //     photon_pipi->outputPhoton_SpvnpT_shell(output_path);
//     //     photon_rhoK->outputPhoton_SpvnpT_shell(output_path);
//     //     photon_rho->outputPhoton_SpvnpT_shell(output_path);
//     //     photon_pirho_omegat->outputPhoton_SpvnpT_shell(output_path);
//     // }

}


void PhotonEmission::calPhoton_total_SpMatrix() {
    for (int k = 0; k < nrapidity; k++) {
        for (int l = 0; l < np; l++) {
            for (int m = 0; m < nphi; m++) {
                for (int j = 0; j < nm; j++) {
                    dNd2pTdphidy_eq[j][l][m][k] = (
                          dilepton_QGP_LO->getPhotonSpMatrix_eq(j, l, m, k));
                    dNd2pTdphidy[j][l][m][k] = (
                          dilepton_QGP_LO->getPhotonSpMatrix_tot(j, l, m, k));

                    // if(dNd2pTdphidy[j][l][m][k]<0.0)
                    //     printf("WARNING, negative dNd2pTdphidy[j][l][m][k]...\n");
                    // if(dNd2pTdphidy_eq[j][l][m][k]<0.0)
                    //     printf("WARNING, negative dNd2pTdphidy_eq[j][l][m][k]...\n");
                    //printf("calPhoton_total_SpMatrix BII...%e\n",dNd2pTdphidy_eq[j][l][m][k]);
                // dNd2pTdphidy_eq[l][m][k] = (
                //       photon_QGP_2_to_2->getPhotonSpMatrix_eq(l, m, k)
                //     + photon_QGP_collinear->getPhotonSpMatrix_eq(l, m, k));
                    // + photon_HG_meson->getPhotonSpMatrix_eq(l, m, k)
                    // + photon_HG_omega->getPhotonSpMatrix_eq(l, m, k)
                    // + photon_HG_rho_spectralfun->getPhotonSpMatrix_eq(l, m, k)
                    // + photon_HG_pipiBremsstrahlung->
                    //                         getPhotonSpMatrix_eq(l, m, k));

                // dNd2pTdphidy[l][m][k] = (
                //       photon_QGP_2_to_2->getPhotonSpMatrix_tot(l, m, k)
                //     + photon_QGP_collinear->getPhotonSpMatrix_tot(l, m, k));
                    // + photon_HG_meson->getPhotonSpMatrix_tot(l, m, k)
                    // + photon_HG_omega->getPhotonSpMatrix_tot(l, m, k)
                    // + photon_HG_rho_spectralfun->getPhotonSpMatrix_tot(l, m, k)
                    // + photon_HG_pipiBremsstrahlung->
                    //                     getPhotonSpMatrix_tot(l, m, k));
                }
            }
        }
    }
}


void PhotonEmission::calPhoton_total_Spvn() {

    for (int m = 0; m < nm; m++) {
        //double Mll = photon_QGP_2_to_2->getDileptonMass(m);
        for (int i = 0; i < np; i++) {
            double p = photon_QGP_2_to_2->getPhotonp(i);
            double pweight = photon_QGP_2_to_2->getPhoton_pweight(i);
            for (int j = 0; j < nphi; j++) {
                double phi = photon_QGP_2_to_2->getPhotonphi(j);
                double phiweight = photon_QGP_2_to_2->getPhoton_phiweight(j);
                double dy = photon_QGP_2_to_2->get_dy();
                double weight = phiweight*dy;
                for (int k = 0; k < nrapidity; k++) {
                    // integrate over rapidity and azimuthal angle of momentum
                    dNd2pTd2M_eq[m][i] += dNd2pTdphidy_eq[m][i][j][k]*weight;
                    dNd2pTd2M[m][i] += dNd2pTdphidy[m][i][j][k]*weight;
                    for (int order = 0; order < norder; order++) {
                        vnpT_cos_eq[order][m][i] += (
                                dNd2pTdphidy_eq[m][i][j][k]*cos(order*phi)*weight);
                        vnpT_cos[order][m][i] += (
                                dNd2pTdphidy[m][i][j][k]*cos(order*phi)*weight);
                        vnpT_sin_eq[order][m][i] += (
                                dNd2pTdphidy_eq[m][i][j][k]*sin(order*phi)*weight);
                        vnpT_sin[order][m][i] += (
                                dNd2pTdphidy[m][i][j][k]*sin(order*phi)*weight);
                    }
                }
            }
            dNd2Mdy_eq[m] += dNd2pTd2M_eq[m][i]*p*pweight; // integral over p
            dNd2Mdy_tot[m] += dNd2pTd2M[m][i]*p*pweight;

            for (int order=0; order < norder; order++) {
                // sum over pT
                vn_cos_eq[order][m] += vnpT_cos_eq[order][m][i]*p*pweight;
                vn_sin_eq[order][m] += vnpT_sin_eq[order][m][i]*p*pweight;
                vn_cos[order][m] += vnpT_cos[order][m][i]*p*pweight;
                vn_sin[order][m] += vnpT_sin[order][m][i]*p*pweight;

                vnpT_cos_eq[order][m][i] = vnpT_cos_eq[order][m][i]/dNd2pTd2M_eq[m][i];
                vnpT_cos[order][m][i] = vnpT_cos[order][m][i]/dNd2pTd2M[m][i];
                vnpT_sin_eq[order][m][i] = vnpT_sin_eq[order][m][i]/dNd2pTd2M_eq[m][i];
                vnpT_sin[order][m][i] = vnpT_sin[order][m][i]/dNd2pTd2M[m][i];
            }
            dNd2pTd2M_eq[m][i] = dNd2pTd2M_eq[m][i]/(2*M_PI); // dN/(2pi pTdpT)
            dNd2pTd2M[m][i] = dNd2pTd2M[m][i]/(2*M_PI);
            // if(dNd2pTd2M_eq[m][i]<0.0)
            //             printf("WARNING, negative dNd2pTd2M_eq[m][i]...\n");
        }
    }
    
    for (int m = 0; m < nm; m++) {
        for (int order = 1; order < norder ; order++) {
            vn_cos_eq[order][m] = vn_cos_eq[order][m]/dNd2Mdy_eq[m];
            vn_sin_eq[order][m] = vn_sin_eq[order][m]/dNd2Mdy_eq[m];
            vn_cos[order][m] = vn_cos[order][m]/dNd2Mdy_tot[m];
            vn_sin[order][m] = vn_sin[order][m]/dNd2Mdy_tot[m];
        }
    }
}


void PhotonEmission::outputPhoton_total_SpMatrix_and_SpvnpT() {
    ostringstream filename_stream_eq_SpMatrix;
    ostringstream filename_stream_eq_Spvn;
    ostringstream filename_stream_SpMatrix;
    ostringstream filename_stream_Spvn;
    ostringstream filename_stream_inte_eq_Spvn;
    ostringstream filename_stream_inte_Spvn;

    string filename = "photon_total";

    filename_stream_eq_SpMatrix << output_path << filename
                                << "_eq_SpMatrix.dat";
    filename_stream_eq_Spvn << output_path << filename << "_eq_Spvn.dat";
    filename_stream_SpMatrix << output_path << filename << "_SpMatrix.dat";
    filename_stream_Spvn << output_path << filename << "_Spvn.dat";
    filename_stream_inte_eq_Spvn << output_path << filename
                                 << "_eq_Spvn_inte.dat";
    filename_stream_inte_Spvn << output_path << filename << "_Spvn_inte.dat";

    ofstream fphoton_eq_SpMatrix(filename_stream_eq_SpMatrix.str().c_str());
    ofstream fphoton_eq_Spvn(filename_stream_eq_Spvn.str().c_str());
    ofstream fphotonSpMatrix(filename_stream_SpMatrix.str().c_str());
    ofstream fphotonSpvn(filename_stream_Spvn.str().c_str());
    ofstream fphotoninte_eq_Spvn(filename_stream_inte_eq_Spvn.str().c_str());
    ofstream fphotoninteSpvn(filename_stream_inte_Spvn.str().c_str());

    for (int m = 0; m < nm; m++) {
        for (int i=0; i < nphi; i++) {
            double phi = photon_QGP_2_to_2->getPhotonphi(i);
            fphoton_eq_SpMatrix << phi << "  ";
            fphotonSpMatrix << phi << "  ";
            for (int j = 0; j < np; j++) {
                double temp_eq = 0.0;
                double temp_tot = 0.0;
                double dy = photon_QGP_2_to_2->get_dy();
                for (int k = 0; k < nrapidity; k++) {
                    temp_eq += dNd2pTdphidy_eq[m][j][i][k]*dy;
                    temp_tot += dNd2pTdphidy[m][j][i][k]*dy;
                }
                fphoton_eq_SpMatrix << scientific << setprecision(6) << setw(16)
                                    << temp_eq << "  ";
                fphotonSpMatrix << scientific << setprecision(6) << setw(16)
                                << temp_tot << "  ";
            }
            fphoton_eq_SpMatrix << endl;
            fphotonSpMatrix << endl;
        }
    }

    // pT differential
    for (int m = 0; m < nm; m++) {
        for (int i = 0; i < np; i++) {
            double M_ll = photon_QGP_2_to_2->getDileptonMass(m);
            fphoton_eq_Spvn << scientific << setprecision(6) << setw(16)
                            << M_ll << "  ";
            fphotonSpvn << scientific << setprecision(6) << setw(16)
                        << M_ll << "  ";
            double pT = photon_QGP_2_to_2->getPhotonp(i);
            fphoton_eq_Spvn << scientific << setprecision(6) << setw(16)
                            << pT << "  " << dNd2pTd2M_eq[m][i] << "  ";
            fphotonSpvn << scientific << setprecision(6) << setw(16)
                        << pT << "  " << dNd2pTd2M[m][i] << "  ";
            for (int order=1; order < norder; order++) {
                fphoton_eq_Spvn << scientific << setprecision(6) << setw(16)
                                << order << "   " << vnpT_cos_eq[order][m][i] << "  "
                                << vnpT_sin_eq[order][m][i] << "  "
                                << sqrt(pow(vnpT_cos_eq[order][m][i], 2)
                                        + pow(vnpT_sin_eq[order][m][i], 2)) << "  ";
                fphotonSpvn << scientific << setprecision(6) << setw(16)
                            << order << "   " << vnpT_cos[order][m][i] << "  "
                            << vnpT_sin[order][m][i] << "  "
                            << sqrt(pow(vnpT_cos[order][m][i], 2)
                                    + pow(vnpT_sin[order][m][i], 2)) << "  ";
            }
            fphoton_eq_Spvn << endl;
            fphotonSpvn << endl;
            // if(dNd2pTd2M_eq[m][i]<0.0)
            //             printf("WARNING, write negative dNd2pTd2M_eq[m][i]...\n");
        }
    }

    // pT integrated
    for (int m = 0; m < nm; m++) {
        double M_ll = photon_QGP_2_to_2->getDileptonMass(m);
        fphotoninte_eq_Spvn << scientific << setprecision(6) << setw(16)
                        << M_ll << "  " << dNd2Mdy_eq[m]*M_ll << "  ";
        fphotoninteSpvn << scientific << setprecision(6) << setw(16)
                    << M_ll << "  " << dNd2Mdy_tot[m]*M_ll << "  ";

        for (int order = 0; order < norder; order++) {
            fphotoninte_eq_Spvn << scientific << setprecision(6) << setw(16)
                                << order << "   " << vn_cos_eq[order][m] << "   "
                                << vn_sin_eq[order][m] << "   "
                                << sqrt(pow(vn_cos_eq[order][m], 2)
                                        + pow(vn_sin_eq[order][m], 2)) << "  ";
            fphotoninteSpvn << scientific << setprecision(6) << setw(16)
                            << order << "   " << vn_cos[order][m] << "   "
                            << vn_sin[order][m] << "   "
                            << sqrt(pow(vn_cos[order][m], 2)
                                    + pow(vn_sin[order][m], 2)) << "  ";
        }
        fphotoninte_eq_Spvn << endl;
        fphotoninteSpvn << endl;
    }
}
