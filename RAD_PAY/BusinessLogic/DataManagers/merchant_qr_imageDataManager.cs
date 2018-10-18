using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_qr_imageDataManager
    {
        //merchant_qr_imageViewModel
        //public int    merchant_id { get; set; }
        //public string location    { get; set; }

        public static void Add(merchant_qr_imageViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new merchant_qr_image
            {
                merchant_id = model.merchant_id,
                location    = model.location   ,
            };

            db.merchant_qr_image.Add(dbmodel);
        }

        public static void Modify(merchant_qr_imageViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_qr_image.Where(z => z.merchant_id == model.merchant_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.merchant_id = model.merchant_id;
                    dbmodel.location = model.location;
                }
            }
        }

        public static void Delete(merchant_qr_imageViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_qr_image.Where(z => z.merchant_id == model.merchant_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.merchant_qr_image.Remove(dbmodel);
                }
            }
        }

        public static List<merchant_qr_imageViewModel> Get(merchant_qr_imageViewModel model, RAD_PAYEntities db)
        {
            List<merchant_qr_imageViewModel> list = null;

            var query = from resmodel in db.merchant_qr_image
                        select new merchant_qr_imageViewModel
                        {
                            merchant_id = resmodel.merchant_id,
                            location = resmodel.location,
                        };

            list = query.ToList();

            return list;
        }
    }
}