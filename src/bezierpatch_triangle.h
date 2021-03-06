#ifndef BEZIERPATCH_TRIANGLE_H
#define BEZIERPATCH_TRIANGLE_H

#include "bezierpatch.h"
#include "productpolynom3var.h"

class BezierPatch_Triangle : public BezierPatch
{
public:
    BezierPatch_Triangle();

    BezierPatch_Triangle(size_t n);
    ~BezierPatch_Triangle();

    //get

    size_t size() const;

    const glm::vec3 &getPoint(size_t i, size_t j, size_t k) const;

    //set

    void setPoint(size_t i, size_t j, size_t k, const glm::vec3& cp, bool sendToVBO=false);

    void makePatchEBO();
    void makeSurfaceVBO();
    void makeSurfaceEBO();

    //operators

    BezierPatch_Triangle& operator=(const BezierPatch_Triangle& other);

    //static random generation

    static BezierPatch_Triangle* generate(size_t n, float xStep=1.0f, float yStep=1.0f, float max_noise=0.5f);

    //constraint solving

    void setFromR2TDependency(const glm::vec3& rectangleCP, const Sum_ProductPolynom3Var& dependency);

protected:

    //others

    void drawPatch() const;
    void drawSurface() const;

    const glm::vec3 &casteljau(float u, float v, float w);

    glm::vec3 &getTmpCasteljau(size_t i, size_t j, size_t k);

private:

    /**
     * @brief calculates \sum_{i=0}^{k}(n-i) for any k>0 and 0 for k=0. n is m_size.
     * This is used to access the vector m_points.
     */
    unsigned int indexOf(unsigned int k) const;

    size_t m_size;

    std::vector<glm::vec3> m_tmpCasteljau;

    /*

    The patch is constructed following this nice ascii blueprint:

       (0,0,n-1)
          /\
         /  \
        /    \
       /      \
      /        \
     /          \
    _____________ (0,n-1,0)
 (n-1,0,0)

    */
};

#endif // BEZIERPATCH_TRIANGLE_H
