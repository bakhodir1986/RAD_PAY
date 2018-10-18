using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class paynet_infoDataManager
    {
        //paynet_infoViewModel
        //public int    id          { get; set; }
        //public long?  rad_tr_id   { get; set; }

        public static void Add(paynet_infoViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new paynet_info
            {
                id          = model.id          ,   
                rad_tr_id   = model.rad_tr_id   ,
            };

            db.paynet_info.Add(dbmodel);
        }

        public static void Modify(paynet_infoViewModel model, RAD_PAYEntities db)
        {
            var result = db.paynet_info.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                   ;
                    dbmodel.rad_tr_id = model.rad_tr_id;
                }
            }
        }

        public static void Delete(paynet_infoViewModel model, RAD_PAYEntities db)
        {
            var result = db.paynet_info.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.paynet_info.Remove(dbmodel);
                }
            }
        }

        public static List<paynet_infoViewModel> Get(paynet_infoViewModel model, RAD_PAYEntities db)
        {
            List<paynet_infoViewModel> list = null;

            var query = from resmodel in db.paynet_info
                        select new paynet_infoViewModel
                        {
                            id = resmodel.id,
                            rad_tr_id = resmodel.rad_tr_id,
                        };

            list = query.ToList();

            return list;
        }
    }
}