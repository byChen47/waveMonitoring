/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | www.openfoam.com
     \\/     M anipulation  |
-------------------------------------------------------------------------------
    Copyright (C) 2026 byChen47
-------------------------------------------------------------------------------
License
    This file is part of waves2Foam (waveMonitoring module).

    waves2Foam is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    waves2Foam is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with waves2Foam.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "waveElevation.H"
#include "addToRunTimeSelectionTable.H"
#include "fvMesh.H"
#include "Time.H"
#include "interpolation.H"
#include "meshSearch.H"
#include "midPointAndFaceSet.H"
#include "uniformDimensionedFields.H"
#include "volFields.H"
#include "mapPolyMesh.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
namespace functionObjects
{
    defineTypeNameAndDebug(waveElevation, 0);
    addToRunTimeSelectionTable(functionObject, waveElevation, dictionary);
}
}


// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

void Foam::functionObjects::waveElevation::writeHeader()
{
    if (!Pstream::master() || !filePtr_.valid())
    {
        return;
    }

    OFstream& os = filePtr_();

    os  << "# waveElevation: free-surface elevation (integrated liquid height)" << nl
        << "# functionObject: " << name() << nl
        << "# alpha: " << alphaName_ << nl
        << "# liquid: " << liquid_ << nl
        << "# direction: " << direction_ << nl
        << "# subtractReference: " << subtractReference_ << nl
        << "# fixedLocations: " << fixedLocations_ << nl
        << "# nGauges: " << locations_.size() << nl;

    forAll(locations_, li)
    {
        os  << "# location " << li << ": " << locations_[li] << nl;
    }

    os  << "# time";
    forAll(locations_, li)
    {
        os  << tab << "eta" << li;
    }
    os  << nl;
}


void Foam::functionObjects::waveElevation::openFile()
{
    const fileName dir =
        mesh_.time().globalPath()/"postProcessing"/"waveElevation"/name();

    mkDir(dir);

    filePtr_.reset(new OFstream(dir/"elevation.dat"));

    writeHeader();
}


Foam::scalar Foam::functionObjects::waveElevation::elevation
(
    const point& loc,
    const vector& gHat
) const
{
    const volScalarField& alpha =
        mesh_.lookupObject<volScalarField>(alphaName_);

    autoPtr<interpolation<scalar>> interpolator
    (
        interpolation<scalar>::New(interpolationScheme_, alpha)
    );

    // Sampling ray through the gauge, spanning the whole bounding box
    const scalar span = mesh_.bounds().mag();

    const midPointAndFaceSet set
    (
        "",
        mesh_,
        meshSearch(mesh_),
        "xyz",
        loc + gHat*span,
        loc - gHat*span
    );

    // Integrate length and length*alpha along the ray
    scalar sumLength = 0;
    scalar sumLengthAlpha = 0;

    for (label si = 0; si < set.size() - 1; ++si)
    {
        if (set.segments()[si] != set.segments()[si+1])
        {
            continue;
        }

        const vector& p0 = set[si];
        const vector& p1 = set[si+1];

        const label c0 = set.cells()[si];
        const label c1 = set.cells()[si+1];

        const label f0 = set.faces()[si];
        const label f1 = set.faces()[si+1];

        const scalar a0 = interpolator->interpolate(p0, c0, f0);
        const scalar a1 = interpolator->interpolate(p1, c1, f1);

        const scalar l = -gHat & (p1 - p0);

        sumLength += l;
        sumLengthAlpha += l*(a0 + a1)/2;
    }

    reduce(sumLength, sumOp<scalar>());
    reduce(sumLengthAlpha, sumOp<scalar>());

    return liquid_ ? sumLengthAlpha : sumLength - sumLengthAlpha;
}


void Foam::functionObjects::waveElevation::attachGauges(const List<point>& locs)
{
    cellIds_.setSize(locs.size(), -1);
    cellC0_.setSize(locs.size(), Zero);

    if (fixedLocations_)
    {
        // Fixed in space: the cell is searched again on every write
        return;
    }

    meshSearch searchEngine(mesh_);

    forAll(locs, li)
    {
        const label celli = searchEngine.findCell(locs[li]);

        cellIds_[li] = celli;

        if (celli >= 0)
        {
            cellC0_[li] = mesh_.C()[celli];
        }
        else
        {
            WarningInFunction
                << "gauge " << li << " at " << locs[li]
                << " is not inside the mesh - the gauge will not move"
                << endl;
        }
    }
}


Foam::List<Foam::point>
Foam::functionObjects::waveElevation::currentLocations() const
{
    List<point> result(locations_);

    forAll(result, li)
    {
        // Displacement of the cell the gauge is attached to, reduced over
        // the processors (only the owner finds the cell)
        vector disp = Zero;
        label n = 0;

        if (cellIds_[li] >= 0)
        {
            disp = mesh_.C()[cellIds_[li]] - cellC0_[li];
            n = 1;
        }

        reduce(disp, sumOp<vector>());
        reduce(n, sumOp<label>());

        if (n > 0)
        {
            result[li] += disp/scalar(n);
        }
    }

    return result;
}


void Foam::functionObjects::waveElevation::updateMesh(const mapPolyMesh& mpm)
{
    if (fixedLocations_)
    {
        return;
    }

    // Topology changed: re-attach at the last known positions and make them
    // the new anchor, so that the displacement stays consistent
    const List<point> locs =
    (
        lastLocations_.size() == locations_.size()
      ? lastLocations_
      : locations_
    );

    attachGauges(locs);

    locations_ = locs;
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::functionObjects::waveElevation::waveElevation
(
    const word& name,
    const Time& runTime,
    const dictionary& dict
)
:
    fvMeshFunctionObject(name, runTime, dict),
    liquid_(true),
    alphaName_("alpha.water"),
    interpolationScheme_("cellPoint"),
    direction_(vector::zero),
    locations_(),
    subtractReference_(false),
    reference_(),
    haveReference_(false),
    fixedLocations_(true),
    cellIds_(),
    cellC0_(),
    lastLocations_(),
    filePtr_()
{
    read(dict);

    openFile();
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::functionObjects::waveElevation::read(const dictionary& dict)
{
    // Pick up the function-object level switches (log, ...)
    functionObject::read(dict);

    dict.readIfPresent("alpha", alphaName_);
    dict.readIfPresent("liquid", liquid_);
    dict.readIfPresent("interpolationScheme", interpolationScheme_);
    dict.readIfPresent("direction", direction_);
    dict.readIfPresent("subtractReference", subtractReference_);

    dict.readIfPresent("fixedLocations", fixedLocations_);

    dict.readEntry("locations", locations_);

    reference_.setSize(locations_.size(), Zero);
    haveReference_ = false;

    attachGauges(locations_);

    return true;
}


bool Foam::functionObjects::waveElevation::execute()
{
    return true;
}


bool Foam::functionObjects::waveElevation::write()
{
    if (locations_.empty())
    {
        return true;
    }

    // Make sure the alpha field exists before sampling
    mesh_.lookupObject<volScalarField>(alphaName_);

    vector gHat(vector::zero);

    if (mag(direction_) > SMALL)
    {
        gHat = direction_/mag(direction_);
    }
    else
    {
        gHat = vector::zero;

        if (mesh_.time().foundObject<uniformDimensionedVectorField>("g"))
        {
            const uniformDimensionedVectorField& g =
                mesh_.time().lookupObject<uniformDimensionedVectorField>("g");

            if (mag(g.value()) > SMALL)
            {
                gHat = g.value()/mag(g.value());
            }
        }

        if (mag(gHat) < SMALL)
        {
            // Fall back to a vertical (upwards) interface normal
            gHat = vector(0, -1, 0);
        }
    }

    // Gauge positions: fixed in space, or moved with the mesh
    const List<point> locs =
    (
        fixedLocations_ ? locations_ : currentLocations()
    );

    lastLocations_ = locs;

    scalarField eta(locs.size(), Zero);

    forAll(locs, li)
    {
        eta[li] = elevation(locs[li], gHat);
    }

    if (subtractReference_)
    {
        if (!haveReference_)
        {
            reference_ = eta;
            haveReference_ = true;
        }

        eta -= reference_;
    }

    if (Pstream::master() && filePtr_.valid())
    {
        OFstream& os = filePtr_();

        os  << mesh_.time().timeOutputValue();

        forAll(eta, li)
        {
            os  << tab << eta[li];
        }

        os  << nl;

        if (log)
        {
            Info<< "waveElevation " << name() << ": t = "
                << mesh_.time().timeOutputValue() << "  eta = " << eta;

            if (!fixedLocations_)
            {
                Info<< "  locations = " << locs;
            }

            Info<< endl;
        }
    }

    return true;
}


// ************************************************************************* //
